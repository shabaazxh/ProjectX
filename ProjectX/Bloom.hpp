#pragma once
#include <volk/volk.h>
#include <vector>
#include "Image.hpp"
#include "Buffer.hpp"

namespace vk
{
	class Context;

	class Bloom
	{
	public:
		explicit Bloom(Context& context, Image& inputImage);
		~Bloom();

		void Execute(VkCommandBuffer cmd);
		void Update();
		void Resize();

		Image& GetRenderTarget() { return m_BloomBlurYRT; }

	private:
		void CreatePipeline();
		void BuildHorizontalBlurDescriptors();
		void BuildVerticalBlurDescriptors();
		void CreateFramebuffer();
		void CreateRenderPass();

		void RenderHorizontalBlur(VkCommandBuffer cmd);
		void RenderVerticalBlur(VkCommandBuffer cmd);

		Context& context;
		Image m_BloomBlurXRT;
		Image m_BloomBlurYRT;
		Image& inputImage;

		std::vector<float> m_weights;

		VkRenderPass m_renderPass;
		VkFramebuffer m_HorizontalBlurFramebuffer;
		VkFramebuffer m_VerticalBlurFramebuffer;

		VkPipeline m_HorizontalBlurPipeline;
		VkPipelineLayout m_HorizontalBlurPipelineLayout;
		std::vector<VkDescriptorSet> m_HorizontalBlurDescriptorSets;
		VkDescriptorSetLayout m_HorizontalBlurDescriptorSetLayout;


		VkPipeline m_VerticalBlurPipeline;
		VkPipelineLayout m_VerticalBlurPipelineLayout;
		std::vector<VkDescriptorSet> m_VerticalBlurDescriptorSets;
		VkDescriptorSetLayout m_VerticalBlurDescriptorSetLayout;

		uint32_t m_width;
		uint32_t m_height;
		Buffer m_GPUWeightsBuffer;
	};
}