#pragma once
#include <volk/volk.h>
#include <memory>
#include "Camera.hpp"
#include "GBuffer.hpp"
#include "Light.hpp"
#include "Scene.hpp"

namespace vk
{
	class Context;
	class Buffer;

	class DefLighting
	{
	public:
		explicit DefLighting(Context& context, std::shared_ptr<Camera>& camera, GBuffer::GBufferMRT& GBufferMRT, Image& shadowMap, std::shared_ptr<Scene> scene);
		~DefLighting();

		void Execute(VkCommandBuffer cmd);
		void Update();
		void Resize();

		Image& GetRenderTarget() { return m_RenderTarget; }
		Image& GetBrightnessRenderTarget() { return m_RenderTargetBrightness; }

	private:
		void CreatePipeline();
		void CreateRenderPass();
		void CreateFramebuffer();
		void BuildDescriptors();

		Context& context;
		Image m_RenderTarget;
		Image m_RenderTargetBrightness;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
		std::vector<VkDescriptorSet> m_descriptorSets;
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkRenderPass m_renderPass;
		VkFramebuffer m_framebuffer;

		uint32_t m_width;
		uint32_t m_height;

		GBuffer::GBufferMRT& GBufferMRT;
		Image& m_shadowMap;

		std::shared_ptr<Scene> scene;
		std::shared_ptr<Camera> camera;		
	};
}