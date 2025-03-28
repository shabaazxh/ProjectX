#pragma once
#include <volk/volk.h>
#include "Image.hpp"
#include "Buffer.hpp"
#include "baked_model.hpp"
#include "Camera.hpp"

namespace vk
{
	class Context;
	class Skybox
	{
	public:
		Skybox(Context& context, const Image& depthBuffer, std::shared_ptr<Camera> camera, VkRenderPass renderpass);
		~Skybox();

		void Execute(VkCommandBuffer cmd);
        void Resize();

	private:

        void CreatePipeline();
        void BuildDescriptors();
        void CreateRenderPass();
        void CreateFramebuffer();
		void LoadCubemapFace(const std::string facePath, char** pixelData);

		Context& context;
        const Image& depthBuffer;
        std::shared_ptr<Camera> camera;
		Image m_Skybox;

        Image m_RenderTarget;

        VkPipeline m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
        VkRenderPass m_RenderPass;
        VkFramebuffer m_Framebuffer;
        VkDescriptorSetLayout m_DescriptorSetLayout;
        std::vector<VkDescriptorSet> m_descriptorSets;

        Buffer m_vertexBuffer;
	};
}

// https://github.com/KhronosGroup/Vulkan-Tools/blob/main/cube/cube.cpp
static const std::array<vk::Vertex, 36> cubeVertices = {

    vk::Vertex{{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},

    vk::Vertex{{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f,  1.0f,  1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},

    vk::Vertex{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{1.0f, -1.0f,  1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{1.0f,  1.0f,  1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},

    vk::Vertex{{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f, -1.0f,  1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},

    vk::Vertex{{ -1.0f,  1.0f, -1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{  1.0f,  1.0f, -1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{  1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{  1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ -1.0f,  1.0f,  1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ -1.0f,  1.0f, -1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},

    vk::Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f, -1.0f,  1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}},
    vk::Vertex{{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0}},
};