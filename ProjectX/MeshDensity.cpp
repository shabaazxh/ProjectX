#include "Context.hpp"
#include "Scene.hpp"
#include "MeshDensity.hpp"
#include "Pipeline.hpp"
#include "baked_model.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "RenderPass.hpp"
#include "Camera.hpp"

vk::MeshDensity::MeshDensity(Context& context, Image& depthPrepass, std::shared_ptr<Scene>& scene, std::shared_ptr<Camera>& camera) :
	context{ context },
	depthPrepass{ depthPrepass },
	scene{ scene },
	camera{ camera },
	m_renderPass{ VK_NULL_HANDLE },
	m_framebuffer{ VK_NULL_HANDLE },
	m_descriptorSetLayout{VK_NULL_HANDLE},
	m_pipeline{ VK_NULL_HANDLE },
	m_pipelineLayout{VK_NULL_HANDLE}
{
	m_RenderTarget = CreateImageTexture2D(
		"MeshDensity_RT",
		context,
		context.extent.width,
		context.extent.height,
		context.swapchainFormat,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	BuildDescriptors();
	CreateRenderPass();
	CreateFramebuffer();
	CreatePipeline();
}

vk::MeshDensity::~MeshDensity()
{
	m_RenderTarget.Destroy(context.device);

	vkDestroyPipeline(context.device, m_pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, m_pipelineLayout, nullptr);
	
	vkDestroyFramebuffer(context.device, m_framebuffer, nullptr);
	vkDestroyRenderPass(context.device, m_renderPass, nullptr);
	vkDestroyDescriptorSetLayout(context.device, m_descriptorSetLayout, nullptr);
}

void vk::MeshDensity::Resize()
{
	uint32_t width = context.extent.width;
	uint32_t height = context.extent.height;

	vkDestroyFramebuffer(context.device, m_framebuffer, nullptr);

	m_RenderTarget.Destroy(context.device);

	m_RenderTarget = CreateImageTexture2D(
		"MeshDensity_RT",
		context,
		width,
		height,
		context.swapchainFormat,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	CreateFramebuffer();
}

void vk::MeshDensity::Execute(VkCommandBuffer cmd)
{
#ifdef _DEBUG
	RenderPassLabel(cmd, "MeshDensity");
#endif // !DEBUG

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = m_renderPass;
	beginInfo.framebuffer = m_framebuffer;
	beginInfo.renderArea.extent = context.extent;

	VkClearValue clearValues[1];
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	beginInfo.clearValueCount = 1;
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

	// =========================
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[currentFrame], 0, nullptr);

	// Draw front freshes
	scene->RenderFrontMeshes(cmd, m_pipelineLayout);

	// Bind alpha masking pipeline for back meshes : Determine is we're rendering the default scene output, if so use alpha making pipeline else use debug pipelines
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	scene->RenderBackMeshes(cmd, m_pipelineLayout);

	vkCmdEndRenderPass(cmd);

#ifdef _DEBUG
	EndRenderPassLabel(cmd);
#endif // !DEBUG
}

void vk::MeshDensity::CreatePipeline()
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
		.offset = 0,
		.size = sizeof(MeshPushConstants)
	};

	auto meshDensityPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/mesh_density.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/mesh_density.geom.spv", ShaderType::GEOM)
		.AddShader("../Engine/assets/shaders/mesh_density.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {m_descriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipeline = meshDensityPipeline.first;
	m_pipelineLayout = meshDensityPipeline.second;
}

void vk::MeshDensity::CreateRenderPass()
{
	RenderPass builder(context.device, 1);

	m_renderPass = builder
		.AddAttachment(context.swapchainFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_NONE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.SetDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
		.AddColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

		// External -> 0 : Color 
		.AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT)

		// 0 -> External : Color : Wait for color writing to finish on the attachment before the fragment shader tries to read from it 
		.AddDependency(0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT)


		// External -> 0 : Depth
		// Wait for the depth-prepass to finish writing to the depth attachment before this pass uses it for depth comparison 
		.AddDependency(
			VK_SUBPASS_EXTERNAL, 0, 
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)


		 // 0 -> External : Depth
		 // Wait for this pass to finish reading from the depth attachment to occlude fragments before the depth-prepass writes to it
		.AddDependency(0, VK_SUBPASS_EXTERNAL, 
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

		.Build();
}

void vk::MeshDensity::CreateFramebuffer()
{
	// Framebuffer
	std::vector<VkImageView> attachments = { m_RenderTarget.imageView, depthPrepass.imageView };
	VkFramebufferCreateInfo fbcInfo = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_renderPass,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.width = context.extent.width,
		.height = context.extent.height,
		.layers = 1
	};

	VK_CHECK(vkCreateFramebuffer(context.device, &fbcInfo, nullptr, &m_framebuffer), "Failed to create mesh density pass framebuffer.");
}

void vk::MeshDensity::BuildDescriptors()
{
	m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	{
		// Set = 0, binding 0 = cameraUBO, binding = 1 = textures
		std::vector<VkDescriptorSetLayoutBinding> bindings = {
			CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT), // SceneUBO (projection, view etc..)
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
}


