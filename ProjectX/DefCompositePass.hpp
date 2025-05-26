#pragma once
#include <volk/volk.h>
#include <memory>
#include "Image.hpp"
#include <vector>

namespace vk
{
	class Context;

	class DefCompositePass
	{
	public:
	  explicit DefCompositePass(Context& context, Image& defLightingPass, Image& BloomPass, const Image& SSRPass, const Image& SSAOPass);
		~DefCompositePass();

		void Execute(VkCommandBuffer cmd);
		void Update();
		void Resize();

		Image& GetRenderTarget() { return m_RenderTarget; }

	private:
		void CreatePipeline();
		void CreateRenderPass();
		void CreateFramebuffer();
		void BuildDescriptors();

		Context& context;
		Image m_RenderTarget;
		Image& defLightingPass;
		Image& BloomPass;
		const Image& SSRPass;
	    const Image& SSAOPass;
		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
		std::vector<VkDescriptorSet> m_descriptorSets;
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkRenderPass m_renderPass;
		VkFramebuffer m_framebuffer;

		uint32_t m_width;
		uint32_t m_height;
	};
}
