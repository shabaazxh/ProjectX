#pragma once

#include "Context.hpp"
#include "baked_model.hpp"
#include "Image.hpp"
#include "Utils.hpp"
#include "Light.hpp"
#include "Buffer.hpp"

#include <cstddef>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vk
{
	class Scene
	{
	public:

		Scene(Context& context);
		void AddModel(const std::shared_ptr<BakedModel>& model);

		void RenderFrontMeshes(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout);
		void RenderBackMeshes(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout);

		void AddLightSource(Light& LightSource);
		void Update(GLFWwindow* window);

		void Destroy();

		const std::vector<std::shared_ptr<BakedModel>> GetModels() const { return m_models; }
		std::vector<Light>&							   GetLights() { return m_Lights; }
		std::vector<Buffer>&						   GetLightsUBO() { return m_LightUBO; }

	private:
		Context& context;
		std::vector<std::shared_ptr<BakedModel>> m_models;

		std::vector<size_t> m_FrontMeshes;
		std::vector<size_t> m_BackMeshes;
		std::vector<Light>  m_Lights;
		LightBuffer m_LightBuffer;
		std::vector<Buffer> m_LightUBO;
	};
}