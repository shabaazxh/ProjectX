#include "Context.hpp"
#include "Scene.hpp"
#include "ForwardPass.hpp"
#include "Pipeline.hpp"
#include "baked_model.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "RenderPass.hpp"
#include "Camera.hpp"

vk::ForwardPass::ForwardPass(Context& context, Image& shadowMap, Image& depthPrepass, std::shared_ptr<Scene>& scene, std::shared_ptr<Camera>& camera) :
	context{ context },
	shadowMap{ shadowMap },
	depthPrepass{ depthPrepass },
	scene {scene},
	camera{ camera }
{
	m_RenderTarget = CreateImageTexture2D(
		"ForwardPassRT",
		context,
		context.extent.width,
		context.extent.height,
		context.swapchainFormat,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_DepthTarget = CreateImageTexture2D(
		"ForwardPassDepth",
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

	//m_Skybox = std::make_unique<Skybox>(context, m_DepthTarget, camera, m_renderPass);
}

vk::ForwardPass::~ForwardPass()
{
	//m_Skybox.reset();
	m_RenderTarget.Destroy(context.device);
	m_DepthTarget.Destroy(context.device);

	for (auto& pair : m_pipelines)
	{
		vkDestroyPipeline(context.device, pair.second.first, nullptr);
		vkDestroyPipelineLayout(context.device, pair.second.second, nullptr);
	}

	vkDestroyFramebuffer(context.device, m_framebuffer, nullptr);
	vkDestroyRenderPass(context.device, m_renderPass, nullptr);
	if (meshDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context.device, meshDescriptorSetLayout, nullptr);
	}
}

void vk::ForwardPass::Resize()
{
	uint32_t width = context.extent.width;
	uint32_t height = context.extent.height;

	vkDestroyFramebuffer(context.device, m_framebuffer, nullptr);

	m_RenderTarget.Destroy(context.device);
	m_DepthTarget.Destroy(context.device);

	m_RenderTarget = CreateImageTexture2D(
		"ForwardPassRT",
		context,
		width,
		height,
		context.swapchainFormat,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1
	);

	m_DepthTarget = CreateImageTexture2D(
		"ForwardPassDepth",
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

void vk::ForwardPass::Execute(VkCommandBuffer cmd)
{
#ifdef _DEBUG
	RenderPassLabel(cmd, "ForwardPass");
#endif // !DEBUG

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = m_renderPass;
	beginInfo.framebuffer = m_framebuffer;
	beginInfo.renderArea.extent = context.extent;

	VkClearValue clearValues[2];
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };
	beginInfo.clearValueCount = 2;
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

	m_Skybox->Execute(cmd);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[setRenderingPipeline].first);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[setRenderingPipeline].second, 0, 1, &m_descriptorSets[currentFrame], 0, nullptr);

	// Draw front freshes
	scene->RenderFrontMeshes(cmd, m_pipelines[setRenderingPipeline].second);

	// Bind alpha masking pipeline for back meshes : Determine is we're rendering the default scene output, if so use alpha making pipeline else use debug pipelines
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[setRenderingPipeline == 1 ? setAlphaMakingPipeline : setRenderingPipeline].first);
	scene->RenderBackMeshes(cmd, m_pipelines[setRenderingPipeline == 1 ? setAlphaMakingPipeline : setRenderingPipeline].second);
	vkCmdEndRenderPass(cmd);

#ifdef _DEBUG
	EndRenderPassLabel(cmd);
#endif // !DEBUG
}

void vk::ForwardPass::CreatePipeline()
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
		.offset = 0,
		.size = sizeof(MeshPushConstants)
	};

	// Default pipeline
	auto defaultPipelineResult = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/default.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {meshDescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 1, {defaultPipelineResult.first, defaultPipelineResult.second} });

	// Linearized Depth debug pipeline
	auto linearizeDepthPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/linearized_depth.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {meshDescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 2, {linearizeDepthPipeline.first, linearizeDepthPipeline.second} });

	// Mipmap pipeline
	auto mipMapPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/mipmap.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {meshDescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 3, {mipMapPipeline.first, mipMapPipeline.second} });

	// Pd Pipeline
	auto pdPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/pd.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {meshDescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 4, {pdPipeline.first, pdPipeline.second} });


	auto alphaMaskPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/alpha_masking.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {meshDescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 5, {alphaMaskPipeline.first, alphaMaskPipeline.second} });


	// Overdraw is pixels written without any early or any z testing
	// so depth enabled and write is off
	auto overdrawPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/overshading.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ meshDescriptorSetLayout }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState(VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD)
		.SetDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 6, {overdrawPipeline.first, overdrawPipeline.second} });


	auto overShadingPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/default.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/overdraw.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ meshDescriptorSetLayout }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState(VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD)
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 7, {overShadingPipeline.first, overShadingPipeline.second} });

	auto meshDensityPipeline = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/mesh_density.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/mesh_density.geom.spv", ShaderType::GEOM)
		.AddShader("../Engine/assets/shaders/mesh_density.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {meshDescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_renderPass)
		.Build();

	m_pipelines.insert({ 8, {meshDensityPipeline.first, meshDensityPipeline.second} });
}

void vk::ForwardPass::CreateRenderPass()
{
	RenderPass builder(context.device, 1);

	m_renderPass = builder
		.AddAttachment(context.swapchainFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.SetDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.AddColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

		// External -> 0 : Color
		.AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT)

		// 0 -> External : Color : Wait for color writing to finish on the attachment before the fragment shader tries to read from it
		.AddDependency(0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT)


		// External -> 0 : Depth
		// Wait for the depth-prepass to finish writing to the depth attachment before this pass uses it for depth comparison
		//.AddDependency(
		//	VK_SUBPASS_EXTERNAL, 0,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)


		// 0 -> External : Depth
		// Wait for this pass to finish reading from the depth attachment to occlude fragments before the depth-prepass writes to it
		//.AddDependency(0, VK_SUBPASS_EXTERNAL,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		//	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

		// External -> 0 : Depth
		.AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)

		// 0 -> External : Depth
		.AddDependency(0, VK_SUBPASS_EXTERNAL,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
		.Build();
}

void vk::ForwardPass::CreateFramebuffer()
{
	// Framebuffer
	std::vector<VkImageView> attachments = { m_RenderTarget.imageView, m_DepthTarget.imageView };
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

void vk::ForwardPass::BuildDescriptors()
{
	m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	{
		// Set = 0, binding 0 = cameraUBO, binding = 1 = textures
		std::vector<VkDescriptorSetLayoutBinding> bindings = {
			CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT), // SceneUBO (projection, view etc..)
			CreateDescriptorBinding(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT), // Light UBO
			CreateDescriptorBinding(2, 200, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT), // Mesh textures
			CreateDescriptorBinding(3, 1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT), // Anisotropic sampler
			CreateDescriptorBinding(4, 1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT), // Non-anisotropic sampler
			CreateDescriptorBinding(5, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		meshDescriptorSetLayout = CreateDescriptorSetLayout(context, bindings);

		AllocateDescriptorSets(context, context.descriptorPool, meshDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT, m_descriptorSets);
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

	// Light UBO
	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = scene->GetLightsUBO()[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(LightBuffer);
		UpdateDescriptorSet(context, 1, bufferInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
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
		BulkImageUpdate(context, 2, imageInfos, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	}

	// Anisotropic sampler
	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imgInfo = {
			.sampler = repeatSamplerAniso
		};
		UpdateDescriptorSet(context, 3, imgInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_SAMPLER);
	}

	// Non-anisotropic sampler
	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imgInfo = {
			.sampler = repeatSampler
		};
		UpdateDescriptorSet(context, 4, imgInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_SAMPLER);
	}

	for (size_t i = 0; i < (size_t)MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imageInfo = {
			.sampler = clampToEdgeSamplerAniso,
			.imageView = shadowMap.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
		};

		UpdateDescriptorSet(context, 5, imageInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}
}

void vk::ForwardPass::Update()
{

}

