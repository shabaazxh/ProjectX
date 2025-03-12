#pragma once
#include <volk/volk.h>
#include <memory>
#include <unordered_map>
#include "Camera.hpp"

namespace vk
{
	class Context;
	class Scene;
	class Buffer;

	class MeshDensity
	{
	public:

		MeshDensity(Context& context, Image& depthPrepass, std::shared_ptr<Scene>& scene, std::shared_ptr<Camera>& camera);
		~MeshDensity();
		void Execute(VkCommandBuffer cmd);

		void Resize();
		Image& GetRenderTarget() { return m_RenderTarget; }

	private:
		void CreatePipeline();
		void CreateRenderPass();
		void CreateFramebuffer();
		void BuildDescriptors();

		Context& context;
		Image& depthPrepass;
		std::shared_ptr<Scene> scene;
		std::shared_ptr<Camera> camera;
		Image m_RenderTarget;
		VkRenderPass m_renderPass;
		VkFramebuffer m_framebuffer;
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		std::vector<VkDescriptorSet> m_descriptorSets;
	};
}