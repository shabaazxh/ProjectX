#include "Scene.hpp"

vk::Scene::Scene(Context& context) : context(context) {}

void vk::Scene::AddModel(const std::shared_ptr<BakedModel>& model)
{
	// Begin creating GPU texture ( image ) resource for each found texture
	model->loadedTextures.resize(model->textures.size());
	for (size_t i = 0; i < model->loadedTextures.size(); i++)
	{
		const std::string path = model->textures[i].path;
		model->loadedTextures[i] = LoadTextureFromDisk(path, context);
	}

	for (auto& mesh : model->meshes)
	{
		VkDeviceSize vertexSize = sizeof(mesh.vertexData[0]) * mesh.vertexData.size();
		CreateAndUploadBuffer(context, mesh.vertexData.data(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertexBuffer);

		VkDeviceSize indexSize = sizeof(mesh.indices[0]) * mesh.indices.size();
		CreateAndUploadBuffer(context, mesh.indices.data(), indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indexBuffer);

	}

	// Seperate front and back meshes depending on whether or not they have a alpha mask texture id
	for (size_t i = 0; i < model->meshes.size(); i++)
	{
		// If the mesh has no alpha masking textures
		if (model->materials[model->meshes[i].materialId].alphaMaskTextureId == std::numeric_limits<uint32_t>::max())
		{
			m_FrontMeshes.push_back(i);
		}
		else
		{
			m_BackMeshes.push_back(i);
		}
	}

	m_models.push_back(model);


	m_LightUBO.resize(MAX_FRAMES_IN_FLIGHT);
	// Light uniform buffers
	for (auto& buffer : m_LightUBO)
		buffer = CreateBuffer("LightUBO", context, sizeof(LightBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

}

void vk::Scene::RenderFrontMeshes(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout)
{
	for (auto& model : m_models)
	{
		for (auto& index : m_FrontMeshes)
		{
			auto& mesh = model->meshes[index];
			MeshPushConstants pc = {};
			pc.ModelMatrix = glm::mat4(1.0f);
			pc.dTextureID = model->materials[mesh.materialId].baseColorTextureId;
			pc.mTextureID = model->materials[mesh.materialId].metalnessTextureId;
			pc.rTextureID = model->materials[mesh.materialId].roughnessTextureId;
			pc.eTextureID = model->materials[mesh.materialId].emissiveTextureId == 0xffffffff ? -1 : model->materials[mesh.materialId].emissiveTextureId;
			pc.nTextureID = model->materials[mesh.materialId].normalMapTextureId;

			vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(MeshPushConstants), &pc);
			// Set up push constants
			VkDeviceSize offset[] = { 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, offset);
			vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
		}
	}
}


void vk::Scene::RenderBackMeshes(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout)
{
	for (auto& model : m_models)
	{
		for (auto& index : m_BackMeshes)
		{
			auto& mesh = model->meshes[index];
			MeshPushConstants pc = {};
			pc.ModelMatrix = glm::mat4(1.0f);
			pc.dTextureID = model->materials[mesh.materialId].baseColorTextureId;
			pc.mTextureID = model->materials[mesh.materialId].metalnessTextureId;
			pc.rTextureID = model->materials[mesh.materialId].roughnessTextureId;
			pc.eTextureID = model->materials[mesh.materialId].emissiveTextureId == 0xffffffff ? -1 : model->materials[mesh.materialId].emissiveTextureId;

			vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(MeshPushConstants), &pc);
			// Set up push constants
			VkDeviceSize offset[] = { 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.buffer, offset);
			vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
		}
	}
}

void vk::Scene::AddLightSource(Light& LightSource)
{
	m_Lights.push_back(std::move(LightSource));
}

void vk::Scene::Update(GLFWwindow* window)
{
	for (auto& light : m_Lights)
	{
		glm::mat4 ortho = glm::ortho(-9.0f, 9.0f, -9.0f, 9.0f, 0.1f, 105.0f);
		glm::mat4 view = glm::lookAt(glm::vec3(light.position), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0));
		light.LightSpaceMatrix = ortho * view;
	}

	// Fill GPU Data with data defined for the scene 
	for (size_t i = 0; i < m_Lights.size(); i++)
	{
		m_LightBuffer.lights[i].type = static_cast<int>(m_Lights[i].Type);
		m_LightBuffer.lights[i].LightPosition = m_Lights[i].position;
		m_LightBuffer.lights[i].LightColour = m_Lights[i].colour;
		m_LightBuffer.lights[i].LightSpaceMatrix = m_Lights[i].LightSpaceMatrix;
	}

	// Pass the light data to the GPU to update all light properties 
	m_LightUBO[currentFrame].WriteToBuffer(m_LightBuffer, sizeof(LightBuffer));
}

void vk::Scene::Destroy()
{
	for (auto& buffer : m_LightUBO)
	{
		buffer.Destroy(context.device);
	}

	if (!m_models.empty())
	{
		for (auto& model : m_models)
		{
			for (auto& mesh : model->meshes)
			{
				mesh.vertexBuffer.Destroy(context.device);
				mesh.indexBuffer.Destroy(context.device);
			}

			for (auto& texture : model->loadedTextures)
			{
				texture.Destroy(context.device);
			}
		}
	}
}
