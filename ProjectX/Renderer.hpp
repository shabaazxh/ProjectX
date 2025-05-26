#pragma once

// Creates command pool, command buffers
#include <volk/volk.h>
#include <memory>
#include <vector>
#include "DepthPrepass.hpp"
#include "ForwardPass.hpp"
#include "MeshDensity.hpp"
#include "PresentPass.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "GBuffer.hpp"
#include "DefLighting.hpp"
#include "DefCompositePass.hpp"
#include "ShadowMap.hpp"
#include "Bloom.hpp"
#include "SSR.h"
#include "SSAO.hpp"
#include "Skybox.hpp"
#include "ImGuiRenderer.hpp"


//Deferred
namespace vk
{
	class Context;
	class Renderer
	{
	public:
		Renderer() = default;
		Renderer(Context& context);

		void Destroy();

		void Render();
		void Update(double deltaTime);


		static void glfwHandleKeyboard(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
		static void glfwCallbackMotion(GLFWwindow* window, double x, double y);

	private:
		void CreateResources();
		void CreateFences();
		void CreateSemaphores();
		void CreateCommandPool();
		void AllocateCommandBuffers();

		void Submit();
		void Present(uint32_t imageIndex);

	private:
		Context& context;
		std::vector<VkFence> m_Fences;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<VkCommandPool> m_commandPool;

		std::shared_ptr<Scene> m_scene;

		std::unique_ptr<DepthPrepass>     m_DepthPrepass;
		std::unique_ptr<MeshDensity>      m_MeshDensity;
		std::unique_ptr<ForwardPass>	  m_ForwardPass;
		std::unique_ptr<ShadowMap>		  m_ShadowMap;
		std::unique_ptr<GBuffer>		  m_GBuffer;
		std::unique_ptr<DefLighting>	  m_DefLighting;
		std::unique_ptr<Bloom>			  m_Bloom;
		std::unique_ptr<SSR>              m_SSR;
		std::unique_ptr<SSAO>			  m_SSAO;
		std::unique_ptr<Skybox>           m_Skybox;
		std::unique_ptr<DefCompositePass> m_DefComposite;
		std::unique_ptr<PresentPass>	  m_PresentPass;

		std::shared_ptr<Camera> m_camera;
	};
}