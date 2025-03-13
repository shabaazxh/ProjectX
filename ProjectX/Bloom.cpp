#include "Context.hpp"
#include "Bloom.hpp"
#include "Pipeline.hpp"
#include "RenderPass.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <array>
#include "Utils.hpp"

namespace
{
	// Weights computed on the CPU and passed to GPU to allow user control over the gaussian blur
	// Kernel & Sigma can be changed and the weights will be recomputed on the CPU and sent to the GPU
	// Introduces some overhead since CPU data needs to be sent to GPU but we pay the cost once
	// which allows flexibility in changing blur settings
	void ComputeGaussianKernel1D(float sigma, int kernelSize, std::vector<float>& offsets, std::vector<float>& combinedWeights)
	{
		int halfSize = kernelSize / 2;
		std::vector<float> weights;
		float sum = 0.0f;

		for (int x = -halfSize; x <= halfSize; x++)
		{
			float weight = static_cast<float>((1.0f / (2 * M_PI * sigma * sigma)) * std::exp(-(x * x) / (2 * sigma * sigma)));
			weights.push_back(weight);
			sum += weight;
		}

		// Normalize
		for (float& weight : weights)
		{
			weight /= sum;
		}

		// We're doing a 43x43 gaussian blur but performance is poor if we do N^2 (O^2)
		// Performance: Do seperable blur, reducing to 2 * N texel fetch O(n)
		// Performance: Exploit GPU bilinear filter reducing to N / 2 samples

		// If we have 43x43, we have seperable with 43 texture fetches
		// introduce bilinear we get: 1 fetch -> remaining N - 1 = 42 / 2 = 21 = 1 + 21 = 22 taps
		int range = halfSize;
		offsets.push_back(0.0);
		combinedWeights.push_back(weights[range]);

		for (int i = 1; i <= range; i++)
		{
			float weight_1 = weights[range + i];
			float weight_2 = weights[range - i];
			float weight = weight_1 + weight_2;
			float offset_1 = static_cast<float>(i);
			float offset_2 = static_cast<float>(i + 1);
			float offset = (offset_1 * weight_1 + offset_2 * weight_2) / (weight);

			offsets.push_back(offset);
			combinedWeights.push_back(weight);
		}

	}
}

vk::Bloom::Bloom(Context& context, Image& inputImage) :
	context{context},
	inputImage{inputImage},
	m_renderPass{VK_NULL_HANDLE},
	m_HorizontalBlurFramebuffer{VK_NULL_HANDLE},
	m_VerticalBlurFramebuffer{VK_NULL_HANDLE},
	m_HorizontalBlurPipeline{VK_NULL_HANDLE},
	m_HorizontalBlurPipelineLayout{VK_NULL_HANDLE},
	m_HorizontalBlurDescriptorSetLayout{VK_NULL_HANDLE},
	m_VerticalBlurPipeline{VK_NULL_HANDLE},
	m_VerticalBlurPipelineLayout{VK_NULL_HANDLE},
	m_VerticalBlurDescriptorSetLayout{VK_NULL_HANDLE},
	m_width{0},
	m_height{0}
{
	m_width = context.extent.width;
	m_height = context.extent.height;

	m_GPUWeightsBuffer = CreateBuffer("GaussianWeightsBuffer", context, sizeof(GuassianWeightsBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	m_BloomBlurXRT = CreateImageTexture2D(
		"Bloom_Blur_X_RT",
		context,
		m_width,
		m_height,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_BloomBlurYRT = CreateImageTexture2D(
		"Bloom_Blur_Y_RT",
		context,
		m_width,
		m_height,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	std::vector<float> offsets;
	std::vector<float> combinedWeights;
	// Original kernel size is 43x43, the function will handle reducing it down to 22 taps
	ComputeGaussianKernel1D(9.0f, 43, offsets, combinedWeights);

	GuassianWeightsBuffer gaussianWeights = {};

	for (size_t i = 0; i < combinedWeights.size(); i++)
	{
		gaussianWeights.weights[i] = combinedWeights[i];
		gaussianWeights.offsets[i] = offsets[i];
	}

	m_GPUWeightsBuffer.WriteToBuffer(gaussianWeights, sizeof(GuassianWeightsBuffer));

	BuildHorizontalBlurDescriptors();
	BuildVerticalBlurDescriptors();

	CreateRenderPass();
	CreateFramebuffer();
	CreatePipeline();
}

vk::Bloom::~Bloom()
{
	m_BloomBlurXRT.Destroy(context.device);
	m_BloomBlurYRT.Destroy(context.device);

	vkDestroyRenderPass(context.device, m_renderPass, nullptr);
	vkDestroyFramebuffer(context.device, m_HorizontalBlurFramebuffer, nullptr);
	vkDestroyFramebuffer(context.device, m_VerticalBlurFramebuffer, nullptr);

	m_GPUWeightsBuffer.Destroy(context.device);
	vkDestroyPipeline(context.device, m_HorizontalBlurPipeline, nullptr);
	vkDestroyPipelineLayout(context.device, m_HorizontalBlurPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(context.device, m_HorizontalBlurDescriptorSetLayout, nullptr);

	vkDestroyPipeline(context.device, m_VerticalBlurPipeline, nullptr);
	vkDestroyPipelineLayout(context.device, m_VerticalBlurPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(context.device, m_VerticalBlurDescriptorSetLayout, nullptr);
}

void vk::Bloom::CreateFramebuffer()
{
	{
		std::vector<VkImageView> attachments = { m_BloomBlurXRT.imageView };

		VkFramebufferCreateInfo fbcInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = context.extent.width,
			.height = context.extent.height,
			.layers = 1
		};

		VK_CHECK(vkCreateFramebuffer(context.device, &fbcInfo, nullptr, &m_HorizontalBlurFramebuffer), "Failed to create Bloom: Horizontal Blur framebuffer.");
	}

	{
		std::vector<VkImageView> attachments = { m_BloomBlurYRT.imageView };

		VkFramebufferCreateInfo fbcInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = context.extent.width,
			.height = context.extent.height,
			.layers = 1
		};

		VK_CHECK(vkCreateFramebuffer(context.device, &fbcInfo, nullptr, &m_VerticalBlurFramebuffer), "Failed to create Bloom: Horizontal Blur framebuffer.");
	}

}


void vk::Bloom::CreateRenderPass()
{
	RenderPass builder(context.device, 1);

	m_renderPass = builder
		.AddAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

		// External -> 0 : Color
		.AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT)

		// 0 -> External : Color : Wait for color writing to finish on the attachment before the fragment shader tries to read from it
		.AddDependency(0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT)
		.Build();

	context.SetObjectName(context.device, (uint64_t)m_renderPass, VK_OBJECT_TYPE_RENDER_PASS, "BloomRenderPass");
}

// dstStage is where other operations will begin once srcStage is finished
void vk::Bloom::Execute(VkCommandBuffer cmd)
{
	RenderHorizontalBlur(cmd);
	RenderVerticalBlur(cmd);
}

void vk::Bloom::RenderHorizontalBlur(VkCommandBuffer cmd)
{
#ifdef _DEBUG
	RenderPassLabel(cmd, "BloomHorizontalBlur");
#endif

	VkRenderPassBeginInfo rpBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rpBegin.renderPass = m_renderPass;
	rpBegin.framebuffer = m_HorizontalBlurFramebuffer;
	rpBegin.renderArea.extent = { m_width, m_height };

	VkClearValue clearValues[1];
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	rpBegin.clearValueCount = 1;
	rpBegin.pClearValues = clearValues;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_width;
	viewport.height = (float)m_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = { m_width, m_height };
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_HorizontalBlurPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_HorizontalBlurPipelineLayout, 0, 1, &m_HorizontalBlurDescriptorSets[currentFrame], 0, nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_HorizontalBlurPipeline);
	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRenderPass(cmd);

#ifdef _DEBUG
	EndRenderPassLabel(cmd);
#endif // !DEBUG
}

void vk::Bloom::RenderVerticalBlur(VkCommandBuffer cmd)
{

#ifdef _DEBUG
	RenderPassLabel(cmd, "BloomVerticalBlur");
#endif // !DEBUG

	VkRenderPassBeginInfo rpBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rpBegin.renderPass = m_renderPass;
	rpBegin.framebuffer = m_VerticalBlurFramebuffer;
	rpBegin.renderArea.extent = { m_width, m_height };

	VkClearValue clearValues[1];
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	rpBegin.clearValueCount = 1;
	rpBegin.pClearValues = clearValues;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_width;
	viewport.height = (float)m_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = { m_width, m_height };
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VerticalBlurPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VerticalBlurPipelineLayout, 0, 1, &m_VerticalBlurDescriptorSets[currentFrame], 0, nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VerticalBlurPipeline);
	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRenderPass(cmd);

#ifdef _DEBUG
	EndRenderPassLabel(cmd);
#endif // !DEBUG

}

void vk::Bloom::Resize()
{
	m_width = context.extent.width;
	m_height = context.extent.height;

	m_BloomBlurXRT.Destroy(context.device);
	m_BloomBlurYRT.Destroy(context.device);

	vkDestroyFramebuffer(context.device, m_HorizontalBlurFramebuffer, nullptr);
	vkDestroyFramebuffer(context.device, m_VerticalBlurFramebuffer, nullptr);

	m_BloomBlurXRT = CreateImageTexture2D(
		"Bloom_Blur_X_RT",
		context,
		m_width,
		m_height,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_BloomBlurYRT = CreateImageTexture2D(
		"Bloom_Blur_Y_RT",
		context,
		m_width,
		m_height,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	CreateFramebuffer();

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imageInfo = {
			.sampler = clampToEdgeSamplerAniso,
			.imageView = inputImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		UpdateDescriptorSet(context, 0, imageInfo, m_HorizontalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_GPUWeightsBuffer.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(GuassianWeightsBuffer);

		UpdateDescriptorSet(context, 1, bufferInfo, m_HorizontalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// Vertical
	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imageInfo = {
			.sampler = clampToEdgeSamplerAniso,
			.imageView = m_BloomBlurXRT.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		UpdateDescriptorSet(context, 0, imageInfo, m_VerticalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_GPUWeightsBuffer.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(GuassianWeightsBuffer);

		UpdateDescriptorSet(context, 1, bufferInfo, m_VerticalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}
}

void vk::Bloom::Update()
{

}

void vk::Bloom::CreatePipeline()
{
	auto pipelineResult = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::NONE, 0)
		.AddShader("../Engine/assets/shaders/fs_tri.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/bloom_blur_x.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {m_HorizontalBlurDescriptorSetLayout} })
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL) // Turn depth read and write OFF ========
		.SetRenderPass(m_renderPass)
		.Build();

	m_HorizontalBlurPipeline = pipelineResult.first;
	m_HorizontalBlurPipelineLayout = pipelineResult.second;

	auto verticalPipelineResult = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::NONE, 0)
		.AddShader("../Engine/assets/shaders/fs_tri.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/bloom_blur_y.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {m_VerticalBlurDescriptorSetLayout} })
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL) // Turn depth read and write OFF ========
		.SetRenderPass(m_renderPass)
		.Build();


	m_VerticalBlurPipeline = verticalPipelineResult.first;
	m_VerticalBlurPipelineLayout = verticalPipelineResult.second;
}

void vk::Bloom::BuildHorizontalBlurDescriptors()
{
	m_HorizontalBlurDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = {
			CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
			CreateDescriptorBinding(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
		};

		m_HorizontalBlurDescriptorSetLayout = CreateDescriptorSetLayout(context, bindings);
	}

	AllocateDescriptorSets(context, context.descriptorPool, m_HorizontalBlurDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT, m_HorizontalBlurDescriptorSets);

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imageInfo = {
			.sampler = clampToEdgeSamplerAniso,
			.imageView = inputImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		UpdateDescriptorSet(context, 0, imageInfo, m_HorizontalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_GPUWeightsBuffer.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(GuassianWeightsBuffer);

		UpdateDescriptorSet(context, 1, bufferInfo, m_HorizontalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}
}

void vk::Bloom::BuildVerticalBlurDescriptors()
{
	m_VerticalBlurDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = {
			CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
			CreateDescriptorBinding(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		m_VerticalBlurDescriptorSetLayout = CreateDescriptorSetLayout(context, bindings);
	}

	AllocateDescriptorSets(context, context.descriptorPool, m_VerticalBlurDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT, m_VerticalBlurDescriptorSets);

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imageInfo = {
			.sampler = clampToEdgeSamplerAniso,
			.imageView = m_BloomBlurXRT.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		UpdateDescriptorSet(context, 0, imageInfo, m_VerticalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_GPUWeightsBuffer.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(GuassianWeightsBuffer);

		UpdateDescriptorSet(context, 1, bufferInfo, m_VerticalBlurDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

}




