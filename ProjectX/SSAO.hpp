#pragma once
#include <volk/volk.h>
#include <vector>
#include "Image.hpp"
#include "Buffer.hpp"
#include "Camera.hpp"

namespace vk
{
	class Context;

	class SSAO
	{
	public:
		explicit SSAO(Context& context, const Image& depthBuffer, const Image& normalsTexture, std::shared_ptr<Camera> camera);
		~SSAO();

		void Execute(VkCommandBuffer cmd);
		void Update();
		void Resize();

		Image& GetRenderTarget() { return m_RenderTarget; }

	private:
		void CreatePipeline();
		void BuildDescriptors();
		void CreateFramebuffer();
		void CreateRenderPass();
	    void GenerateNoiseTexture(uint32_t width, uint32_t height);

		Context& context;
		const Image& depthBuffer;
		const Image& normalsTexture;
		std::shared_ptr<Camera> camera;
		Image m_RenderTarget;
	    Image m_NoiseTexture;

		uint32_t m_width;
		uint32_t m_height;

		VkRenderPass m_RenderPass;
		VkFramebuffer m_Framebuffer;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		VkDescriptorSetLayout m_DescriptorSetLayout;
		std::vector<Buffer> m_SSAOUniform;
	};
}
