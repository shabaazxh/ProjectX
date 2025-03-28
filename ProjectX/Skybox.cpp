#include "Context.hpp"
#include "Skybox.hpp"
#include "Utils.hpp"
#include "Pipeline.hpp"
#include "RenderPass.hpp"
#include <stb_image.h>

vk::Skybox::Skybox(Context& context, const Image& depthBuffer, std::shared_ptr<Camera> camera, VkRenderPass renderpass) :
	context{context}, depthBuffer{depthBuffer}, camera{camera}, m_RenderPass{renderpass}
{
	// When transitioning this image, the subresource in subresourceRange needs to be set to 6
	// to transition all layers of the imag, otherwise you transition only the first
	m_Skybox = CreateImageTexture2D(
		"Skybox",
		context,
		2048,
		2048,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		6
	);

	char* faceTextureData[6]; // Stores the pixel data from stb
	LoadCubemapFace("assets/Skybox/right.jpg",  &faceTextureData[0]);
	LoadCubemapFace("assets/Skybox/left.jpg",   &faceTextureData[1]);
	LoadCubemapFace("assets/Skybox/top.jpg",    &faceTextureData[2]);
	LoadCubemapFace("assets/Skybox/bottom.jpg", &faceTextureData[3]);
	LoadCubemapFace("assets/Skybox/front.jpg",  &faceTextureData[4]);
	LoadCubemapFace("assets/Skybox/back.jpg",   &faceTextureData[5]);

	constexpr uint32_t width = 2048;
	constexpr uint32_t height = 2048;
	VkDeviceSize imageSize = width * height * 4; // Size of a single face
	VkDeviceSize totalSize = imageSize * 6; // Size of total num of faces

	// Copy the pixel data to the staging buffer to store all face images data
	Buffer stagingBuffer = CreateBuffer("stagingBuffer", context, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	void* data = nullptr;
	if (vmaMapMemory(context.allocator, stagingBuffer.allocation, &data) == VK_SUCCESS)
	{
		uint8_t* bufferData = static_cast<uint8_t*>(data);
		for (uint32_t i = 0; i < 6; i++)
		{
			assert(faceTextureData[i] != nullptr);
			std::memcpy(bufferData + (imageSize * i), faceTextureData[i], imageSize);
		}
	}
	vmaUnmapMemory(context.allocator, stagingBuffer.allocation);

	for (size_t i = 0; i < 6; i++)
	{
		stbi_image_free(faceTextureData[i]);
		faceTextureData[i] = nullptr;
	}

	std::vector<VkBufferImageCopy> copyRegions(6);
	for (uint32_t face = 0; face < 6; face++)
	{
		copyRegions[face].bufferOffset = imageSize * face;
		copyRegions[face].bufferRowLength = 0;
		copyRegions[face].bufferImageHeight = 0;

		copyRegions[face].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegions[face].imageSubresource.mipLevel = 0;
		copyRegions[face].imageSubresource.baseArrayLayer = face;
		copyRegions[face].imageSubresource.layerCount = 1;

		copyRegions[face].imageOffset = { 0,0,0 };
		copyRegions[face].imageExtent = { width, height, 1 };
	}

	ExecuteSingleTimeCommands(context, [&](VkCommandBuffer cmd)
	{
			ImageBarrier(
				cmd,
				m_Skybox.image,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
			);

			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, m_Skybox.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

			ImageBarrier(
				cmd,
				m_Skybox.image,
				VK_ACCESS_TRANSFER_READ_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
			);
	});



	// Create the vertex buffer for the cube map
	VkDeviceSize vertexSize = sizeof(cubeVertices[0]) * cubeVertices.size();
	CreateAndUploadBuffer(context, cubeVertices.data(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBuffer);


	//m_RenderTarget = CreateImageTexture2D(
	//	"Skybox_RT",
	//	context,
	//	context.extent.width,
	//	context.extent.height,
	//	VK_FORMAT_R8G8B8A8_SRGB,
	//	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	//	VK_IMAGE_ASPECT_COLOR_BIT,
	//	1
	//);

	stagingBuffer.Destroy(context.device);

	BuildDescriptors();
	CreatePipeline();
}

vk::Skybox::~Skybox()
{
	m_Skybox.Destroy(context.device);
	m_vertexBuffer.Destroy(context.device);
	vkDestroyPipeline(context.device, m_Pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, m_PipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(context.device, m_DescriptorSetLayout, nullptr);
}

void vk::Skybox::Execute(VkCommandBuffer cmd)
{
#ifdef _DEBUG
	RenderPassLabel(cmd, "Skybox");
#endif // !DEBUG

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_descriptorSets[currentFrame], 0, nullptr);

	MeshPushConstants pc = {};
	pc.ModelMatrix = glm::mat4(1.0f);
	vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(MeshPushConstants), &pc);

	// Set up push constants
	VkDeviceSize offset[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer.buffer, offset);
	vkCmdDraw(cmd, 32, 1, 0, 0);

#ifdef _DEBUG
	EndRenderPassLabel(cmd);
#endif // !DEBUG
}

void vk::Skybox::CreatePipeline()
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
		.offset = 0,
		.size = sizeof(MeshPushConstants)
	};

	auto skyboxPiplineRes = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::BIND, 0)
		.AddShader("../Engine/assets/shaders/skybox.vert.spv", ShaderType::VERTEX)
		.AddShader("../Engine/assets/shaders/skybox.frag.spv", ShaderType::FRAGMENT)
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
		.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetPipelineLayout({ {m_DescriptorSetLayout} }, pushConstantRange)
		.SetSampling(VK_SAMPLE_COUNT_1_BIT)
		.AddBlendAttachmentState()
		.SetDepthState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
		.SetRenderPass(m_RenderPass)
		.Build();

	m_Pipeline = skyboxPiplineRes.first;
	m_PipelineLayout = skyboxPiplineRes.second;
}

void vk::Skybox::BuildDescriptors()
{
	m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	{
		// Set = 0, binding 0 = cameraUBO, binding = 1 = textures
		std::vector<VkDescriptorSetLayoutBinding> bindings = {
			CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
			CreateDescriptorBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		m_DescriptorSetLayout = CreateDescriptorSetLayout(context, bindings);

		AllocateDescriptorSets(context, context.descriptorPool, m_DescriptorSetLayout, MAX_FRAMES_IN_FLIGHT, m_descriptorSets);
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

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorImageInfo imageInfo = {
			.sampler = clampToEdgeSamplerAniso,
			.imageView = m_Skybox.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		UpdateDescriptorSet(context, 1, imageInfo, m_descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}
}

void vk::Skybox::LoadCubemapFace(const std::string facePath, char** pixelData)
{
	int w, h, texChannels;
	stbi_set_flip_vertically_on_load(0);
	stbi_uc* pixels = stbi_load(facePath.c_str(), &w, &h, &texChannels, 4);

	const uint32_t width = static_cast<uint32_t>(w);
	const uint32_t height = static_cast<uint32_t>(h);

	if (!pixels)
	{
		throw std::runtime_error("Failed to load skybox face: " + facePath);
	}

	*pixelData = reinterpret_cast<char*>(pixels);
}


void vk::Skybox::CreateRenderPass()
{
	RenderPass builder(context.device, 1);

	m_RenderPass = builder
		.AddAttachment(VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

		// External -> 0 : Color
		.AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT)

		// 0 -> External : Color : Wait for color writing to finish on the attachment before the fragment shader tries to read from it
		.AddDependency(0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT)
		.Build();

	context.SetObjectName(context.device, (uint64_t)m_RenderPass, VK_OBJECT_TYPE_RENDER_PASS, "SkyboxRenderPass");
}

