#include "Context.hpp"
#include "Scene.hpp"
#include "GBuffer.hpp"
#include "Pipeline.hpp"
#include "baked_model.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "RenderPass.hpp"
#include "Camera.hpp"

vk::GBuffer::GBuffer(Context& context, std::shared_ptr<Scene>& scene, std::shared_ptr<Camera>& camera) : context{ context }, scene{ scene }, camera{ camera }
{
	m_GBufferMRT.AlbedoTarget = CreateImageTexture2D(
		"GBuffer_Albedo_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.NormalTarget = CreateImageTexture2D(
		"GBuffer_Normal_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.MetRoughnessTarget = CreateImageTexture2D(
		"GBuffer_MetRoughness_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_R8G8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.EmissiveTarget = CreateImageTexture2D(
		"GBuffer_Emissive_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_R16G16B16A16_SFLOAT, // HDR format for emissive
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.DepthTarget = CreateImageTexture2D(
		"GBuffer_Depth_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		1
	);

	BuildDescriptors();
	CreateRenderPass();
	CreateFramebuffer();
	CreatePipeline();
}

vk::GBuffer::~GBuffer()
{
	m_GBufferMRT.AlbedoTarget.Destroy(context.device);
	m_GBufferMRT.NormalTarget.Destroy(context.device);
	m_GBufferMRT.MetRoughnessTarget.Destroy(context.device);
	m_GBufferMRT.EmissiveTarget.Destroy(context.device);
	m_GBufferMRT.DepthTarget.Destroy(context.device);

	vkDestroyPipeline(context.device, m_Pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, m_PipelineLayout, nullptr);

	vkDestroyPipeline(context.device, m_AlphaMaskingPipeline, nullptr);
	vkDestroyPipelineLayout(context.device, m_AlphaMaskingPipelineLayout, nullptr);

	vkDestroyFramebuffer(context.device, m_framebuffer, nullptr);
	vkDestroyRenderPass(context.device, m_renderPass, nullptr);
	vkDestroyDescriptorSetLayout(context.device, m_descriptorSetLayout, nullptr);
}

void vk::GBuffer::Resize()
{
	uint32_t width = context.extent.width;
	uint32_t height = context.extent.height;

	vkDestroyFramebuffer(context.device, m_framebuffer, nullptr);

	m_GBufferMRT.AlbedoTarget.Destroy(context.device);
	m_GBufferMRT.NormalTarget.Destroy(context.device);
	m_GBufferMRT.MetRoughnessTarget.Destroy(context.device);
	m_GBufferMRT.EmissiveTarget.Destroy(context.device);
	m_GBufferMRT.DepthTarget.Destroy(context.device);

	m_GBufferMRT.AlbedoTarget = CreateImageTexture2D(
		"GBuffer_Albedo_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.NormalTarget = CreateImageTexture2D(
		"GBuffer_Normal_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.MetRoughnessTarget = CreateImageTexture2D(
		"GBuffer_MetRoughness_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_R8G8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.EmissiveTarget = CreateImageTexture2D(
		"GBuffer_Emissive_RT",
		context,
		context.extent.width,
		context.extent.height,
		VK_FORMAT_R16G16B16A16_SFLOAT, // HDR format for emissive
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_GBufferMRT.DepthTarget = CreateImageTexture2D(
		"GBuffer_Depth_RT",
		context,
		width,
		height,
		VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		1
	);

	CreateFramebuffer();
}

void vk::GBuffer::Execute(VkCommandBuffer cmd)
{

#ifdef _DEBUG
	RenderPassLabel(cmd, "G-Buffer");
#endif // !DEBUG

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = m_renderPass;
	beginInfo.framebuffer = m_framebuffer;
	beginInfo.renderArea.extent = context.extent;

	VkClearValue clearValues[5];
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[3].color = { {0.0f, 0.0f} };
	clearValues[4].depthStencil.depth = { 1.0f };
	beginInfo.clearValueCount = 5;
	beginInfo.pClearValues = clearValues;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)context.extent.width;
	viewport.height = (float)context.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = { context.extent.width, context.extent.height };
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_descriptorSets[currentFrame], 0, nullptr);

	// Draw front freshes
	scene->RenderFrontMeshes(cmd, m_PipelineLayout);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_AlphaMaskingPipeline);
	scene->RenderBackMeshes(cmd, m_AlphaMaskingPipelineLayout);

	vkCmdEndRenderPass(cmd);

#ifdef _DEBUG
	EndRenderPassLabel(cmd);
#endif
}

void vk::GBuffer::CreatePipeline()
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
		.offset = 0,
		.size = sizeof(MeshPushConstants)
	};

	// G-Buffer for non-alpha material meshes
	auto gBufferPipelineRes =
		vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/a12/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/a12/shaders/gbuffer.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {m_descriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.AddBlendAttachmentState()
		.AddBlendAttachmentState()
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_Pipeline		 = gBufferPipelineRes.first;
	m_PipelineLayout = gBufferPipelineRes.second;

	// G-Buffer alpha masking
	auto gBufferAlphaMaskingPipelineRes =
		vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/a12/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/a12/shaders/gbuffer_alpha.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {m_descriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.AddBlendAttachmentState()
		.AddBlendAttachmentState()
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_AlphaMaskingPipeline		 = gBufferAlphaMaskingPipelineRes.first;
	m_AlphaMaskingPipelineLayout = gBufferAlphaMaskingPipelineRes.second;
}

void vk::GBuffer::CreateRenderPass()
{
	RenderPass builder(context.device, 1);

	m_renderPass = builder
		.AddAttachment(VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddAttachment(VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddAttachment(VK_FORMAT_R8G8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)

		.AddColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddColorAttachmentRef(0, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddColorAttachmentRef(0, 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddColorAttachmentRef(0, 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.SetDepthAttachmentRef(0, 4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

		// External -> 0 : Color
		.AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT)

		// 0 -> External : Color : Wait for color writing to finish on the attachment before the fragment shader tries to read from it
		.AddDependency(0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT)

		// External -> 0 : Depth
		.AddDependency(VK_SUBPASS_EXTERNAL, 0,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // Written to during late fragment
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)

		// 0 -> External : Depth
		.AddDependency(0, VK_SUBPASS_EXTERNAL,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)

		.Build();
}

void vk::GBuffer::CreateFramebuffer()
{
	// Framebuffer
	std::vector<VkImageView> attachments = {
		m_GBufferMRT.AlbedoTarget.imageView,
		m_GBufferMRT.NormalTarget.imageView,
		m_GBufferMRT.EmissiveTarget.imageView,
		m_GBufferMRT.MetRoughnessTarget.imageView,
		m_GBufferMRT.DepthTarget.imageView
	};
	VkFramebufferCreateInfo fbcInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_renderPass,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.width = context.extent.width,
		.height = context.extent.height,
		.layers = 1
	};

	VK_CHECK(vkCreateFramebuffer(context.device, &fbcInfo, nullptr, &m_framebuffer), "Failed to create Forward pass framebuffer.");
}

void vk::GBuffer::BuildDescriptors()
{
	m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	{
		// Set = 0, binding 0 = cameraUBO, binding = 1 = textures
		std::vector<VkDescriptorSetLayoutBinding> bindings = {
			CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), // SceneUBO (projection, view etc..)
			CreateDescriptorBinding(1, 200, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT), // Mesh textures
			CreateDescriptorBinding(2, 1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Anisotropic sampler
		};

		m_descriptorSetLayout = CreateDescriptorSetLayout(context, bindings);

		AllocateDescriptorSets(context, context.descriptorPool, m_descriptorSetLayout, MAX_FRAMES_IN_FLIGHT, m_descriptorSets);
	}

	// Camera Transform UBO
	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = camera->GetBuffers()[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(CameraTransform);
		UpdateDescriptorSet(context, 0, bufferInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// Mesh textures
	std::vector<VkDescriptorImageInfo> imageInfos;
	for (auto& model : scene->GetModels())
	{
		for (size_t i = 0; i < model->loadedTextures.size(); i++)
		{
			VkDescriptorImageInfo imgInfo = {
				.sampler = VK_NULL_HANDLE,
				.imageView = model->loadedTextures[i].imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};

			imageInfos.push_back(imgInfo);
		}
	}

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		BulkImageUpdate(context, 1, imageInfos, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	}

	// Anisotropic sampler
	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imgInfo = {
			.sampler = repeatSamplerAniso
		};
		UpdateDescriptorSet(context, 2, imgInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_SAMPLER);
	}
}

void vk::GBuffer::Update()
{

}

