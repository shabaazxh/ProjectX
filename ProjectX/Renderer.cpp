#include "Context.hpp"
#include "Renderer.hpp"
#include "Utils.hpp"
#include "baked_model.hpp"
#include "Light.hpp"

namespace
{
	constexpr glm::vec3 cameraPos = glm::vec3(1.0f, 2.0f, -24.0f);
	constexpr glm::vec3 cameraDir = glm::vec3(1.0f, 1.0f, -1.0f);
	constexpr glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0);
}

vk::Renderer::Renderer(Context& context) : context{context}
{
	vk::renderType = RenderType::DEFERRED;

	CreateResources();

	// Samplers
	repeatSamplerAniso	 	  = CreateSampler(context, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE,  VK_COMPARE_OP_LESS_OR_EQUAL);
	repeatSampler			  = CreateSampler(context, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
	clampToEdgeSamplerAniso   = CreateSampler(context, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE, VK_COMPARE_OP_GREATER);

	// Camera
	m_camera = std::make_shared<Camera>(context, cameraPos, glm::normalize(cameraPos + cameraDir), up, context.extent.width / (float)context.extent.height);

	// GLFW callbacks
	glfwSetWindowUserPointer(context.window, m_camera.get());
	glfwSetKeyCallback(context.window, &glfwHandleKeyboard);
	glfwSetMouseButtonCallback(context.window, glfwMouseButtonCallback);
	glfwSetCursorPosCallback(context.window, glfwCallbackMotion);

	// Define Light sources
	Light directionalLight;
	directionalLight.Type = LightType::Directional;
	directionalLight.position = glm::vec4(0.0f, 1.0f, 0.0, 1.0f); // -0.2972
	directionalLight.colour   = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);


	std::vector<glm::vec4> spotLightPositions = {
		glm::vec4(-5.77367, -0.573576, -12.2166, 1.0f),
		glm::vec4(-7.93594, -0.894144, -15.881, 1.0f),
		glm::vec4(-2.13638, -0.771904, -10.1234, 1.0f),
		glm::vec4(-5.77367, -0.573576, -12.2166, 1.0f),
		glm::vec4(-7.93594, -0.894144, -15.881, 1.0f),
		glm::vec4(-2.13638, -0.771904, -10.1234, 1.0f),
		glm::vec4(2.01896, -0.810065, -10.0543, 1.0f),
		glm::vec4(5.68575, -0.818747, -12.2383, 1.0f),
		glm::vec4(7.99495, -0.842247, -15.983, 1.0f),
		glm::vec4(3.10194, -0.842027, -25.7605, 1.0f),
		glm::vec4(-3.19912, -0.742865, -25.8225, 1.0f),
		glm::vec4(2.45963, -2.85884, -46.4873, 1.0f),
		glm::vec4(-2.51951, -2.86852, -46.545, 1.0f),
		glm::vec4(-0.0964891, -3.64552, -49.5306, 1.0f),
		glm::vec4(-7.53906, -0.704128, -36.1376, 1.0f),
		glm::vec4(-0.142371, 1.18231, -17.2337, 1.0f),
		glm::vec4(-7.29995, -2.83107, -61.3895, 1.0f),
		glm::vec4(7.22173, -2.91155, -61.1362, 1.0f),
		glm::vec4(- 2.0948, -2.90848, -66.6499, 1.0f),
		glm::vec4(2.14284, -2.99084, -66.4837, 1.0f),
		glm::vec4(2.16257, -2.90632, -68.9382, 1.0f),
		glm::vec4(- 2.03022, -2.82303, -68.8924, 1.0f),
		glm::vec4(- 0.0289998, -3.93954, -75.9016, 1.0f),
		glm::vec4(- 1.63065, -2.92373, -88.7963, 1.0f),
		glm::vec4(1.33966, -2.90526, -88.7785, 1.0f)
	};

	// Define and Add to scene
	m_scene = std::make_shared<Scene>(context);
	m_scene->AddModel(std::make_unique<BakedModel>(load_baked_model("assets/a12/suntemple.comp5892mesh_new_packed")));
	m_scene->AddLightSource(directionalLight);

	for (const auto& position : spotLightPositions)
	{
		Light spotLight = {};
		spotLight.Type = LightType::Spot;
		spotLight.position = position;
		spotLight.colour = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
		m_scene->AddLightSource(spotLight);
	}

	std::cout << "Num lights: " << m_scene->GetLights().size() << std::endl;

	// Rendering passes
	m_ShadowMap	   = std::make_unique<ShadowMap>(context, m_scene);
	m_DepthPrepass = std::make_unique<DepthPrepass>(context, m_scene, m_camera);
	m_MeshDensity  = std::make_unique<MeshDensity>(context, m_DepthPrepass->GetRenderTarget(), m_scene, m_camera);
	m_ForwardPass  = std::make_unique<ForwardPass>(context, m_ShadowMap->GetRenderTarget(), m_DepthPrepass->GetRenderTarget(), m_scene, m_camera);
	m_GBuffer	   = std::make_unique<GBuffer>(context, m_scene, m_camera);
	m_DefLighting  = std::make_unique<DefLighting>(context, m_camera, m_GBuffer->GetGBufferMRT(), m_ShadowMap->GetRenderTarget(), m_scene);
	m_Bloom		   = std::make_unique<Bloom>(context, m_DefLighting->GetBrightnessRenderTarget());
	m_DefComposite = std::make_unique<DefCompositePass>(context, m_DefLighting->GetRenderTarget(), m_Bloom->GetRenderTarget());
	m_PresentPass  = std::make_unique<PresentPass>(context, m_ForwardPass->GetRenderTarget(), m_DefComposite->GetRenderTarget(), m_MeshDensity->GetRenderTarget());

	ImGuiRenderer::Initialize(context);
	ImGuiRenderer::AddTexture(clampToEdgeSamplerAniso, m_ShadowMap->GetRenderTarget().imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);
}

void vk::Renderer::Destroy()
{
	vkDeviceWaitIdle(context.device);

	ImGuiRenderer::Shutdown(context);
	m_DepthPrepass.reset();
	m_MeshDensity.reset();
	m_ForwardPass.reset();
	m_ShadowMap.reset();
	m_GBuffer.reset();
	m_DefLighting.reset();
	m_DefComposite.reset();
	m_Bloom.reset();
	m_camera.reset();
	m_scene->Destroy();

	vkDestroySampler(context.device, repeatSamplerAniso, nullptr);
	vkDestroySampler(context.device, repeatSampler, nullptr);
	vkDestroySampler(context.device, clampToEdgeSamplerAniso, nullptr);

	for (auto& fence : m_Fences)
	{
		vkDestroyFence(context.device, fence, nullptr);
	}

	for (auto& semaphore : m_imageAvailableSemaphores)
	{
		vkDestroySemaphore(context.device, semaphore, nullptr);
	}

	for (auto& semaphore : m_renderFinishedSemaphores)
	{
		vkDestroySemaphore(context.device, semaphore, nullptr);
	}

	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkFreeCommandBuffers(context.device, m_commandPool[i], 1, &m_commandBuffers[i]);
	}

	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyCommandPool(context.device, m_commandPool[i], nullptr);
	}
}

void vk::Renderer::CreateResources()
{
	CreateFences();
	CreateSemaphores();
	CreateCommandPool();
	AllocateCommandBuffers();
}

void vk::Renderer::CreateFences()
{
	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Fence
		VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};

		VkFence fence = VK_NULL_HANDLE;
		VK_CHECK(vkCreateFence(context.device, &fenceInfo, nullptr, &fence), "Failedd to create Fence.");
		m_Fences.push_back(std::move(fence));
	}
}

void vk::Renderer::CreateSemaphores()
{
	// Image available semaphore
	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo semaphoreInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		};

		VkSemaphore semaphore = VK_NULL_HANDLE;
		VK_CHECK(vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &semaphore), "Failed to create image available semaphore");
		m_imageAvailableSemaphores.push_back(std::move(semaphore));
	}

	// Render finished sempahore
	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo semaphoreInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		};

		VkSemaphore semaphore = VK_NULL_HANDLE;
		VK_CHECK(vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &semaphore), "Failed to create render finished semaphore");
		m_renderFinishedSemaphores.push_back(std::move(semaphore));
	}
}

void vk::Renderer::CreateCommandPool()
{
	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkCommandPoolCreateInfo cmdPool{};
		cmdPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmdPool.queueFamilyIndex = context.graphicsFamilyIndex;

		VkCommandPool commandPool = VK_NULL_HANDLE;
		VK_CHECK(vkCreateCommandPool(context.device, &cmdPool, nullptr, &commandPool), "Failed to create command pool");
		m_commandPool.push_back(std::move(commandPool));
	}
}

void vk::Renderer::AllocateCommandBuffers()
{
	for (size_t i = 0; i < (size_t)vk::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Allocate command buffers from command pool
		VkCommandBufferAllocateInfo cmdAlloc{};
		cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAlloc.commandPool = m_commandPool[i];
		cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAlloc.commandBufferCount = 1;

		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VK_CHECK(vkAllocateCommandBuffers(context.device, &cmdAlloc, &cmd), "Failed to allocate command buffer");
		m_commandBuffers.push_back(cmd);
	}
}

void vk::Renderer::Render()
{
	vkWaitForFences(context.device, 1, &m_Fences[vk::currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t index;
	VkResult getImageIndex = vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX, m_imageAvailableSemaphores[vk::currentFrame], VK_NULL_HANDLE, &index);

	if (getImageIndex == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// Recreate swapchain
		context.RecreateSwapchain();
		m_DepthPrepass->Resize();
		m_MeshDensity->Resize();
		m_ForwardPass->Resize();
		m_ShadowMap->Resize();
		m_GBuffer->Resize();
		m_DefLighting->Resize();
		m_Bloom->Resize();
		m_DefComposite->Resize();
		m_PresentPass->Resize();
	}
	else if (getImageIndex != VK_SUCCESS && getImageIndex != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to aquire swapchain image");
	}

	vkResetFences(context.device, 1, &m_Fences[vk::currentFrame]);
	vkResetCommandBuffer(m_commandBuffers[vk::currentFrame], 0);

	VkCommandBuffer& cmd = m_commandBuffers[vk::currentFrame];

	{
		VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Failed to begin command buffer");

		m_ShadowMap->Execute(cmd);
		m_DepthPrepass->Execute(cmd);
		if (renderType == RenderType::FORWARD)
		{
			m_ForwardPass->Execute(cmd);

		}
		else if (renderType == RenderType::MESH_DENSITY)
		{
			m_MeshDensity->Execute(cmd);
		}
		else
		{
			m_GBuffer->Execute(cmd);
			m_DefLighting->Execute(cmd);
			m_Bloom->Execute(cmd);
			m_DefComposite->Execute(cmd);
		}

		m_PresentPass->Execute(cmd, index);
		vkEndCommandBuffer(cmd);
	}

	Submit();
	Present(index);

	vk::currentFrame = (vk::currentFrame + 1) % vk::MAX_FRAMES_IN_FLIGHT;
}

void vk::Renderer::Submit()
{
	VkPipelineStageFlags waitStage = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo subtmitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_imageAvailableSemaphores[vk::currentFrame],
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &m_commandBuffers[vk::currentFrame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_renderFinishedSemaphores[vk::currentFrame]
	};

	VkResult result = vkQueueSubmit(context.graphicsQueue, 1, &subtmitInfo, m_Fences[vk::currentFrame]);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit command buffers");
	}

}

void vk::Renderer::Present(uint32_t imageIndex)
{
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_renderFinishedSemaphores[vk::currentFrame],
		.swapchainCount = 1,
		.pSwapchains = &context.swapchain,
		.pImageIndices = &imageIndex,
	};


	VkResult result = vkQueuePresentKHR(context.presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		// Recreate the swapchain
		context.RecreateSwapchain();
		m_DepthPrepass->Resize();
		m_MeshDensity->Resize();
		m_ForwardPass->Resize();
		m_ShadowMap->Resize();
		m_GBuffer->Resize();
		m_DefLighting->Resize();
		m_Bloom->Resize();
		m_DefComposite->Resize();
		m_PresentPass->Resize();
	}
}

void vk::Renderer::Update(double deltaTime)
{
	m_camera->Update(context.window, context.extent.width, context.extent.height, deltaTime);
	m_scene->Update(context.window);

	// Update passes
	ImGuiRenderer::Update(m_scene, m_camera);
	m_ShadowMap->Update();
	m_ForwardPass->Update();
	m_DefLighting->Update();
	m_PresentPass->Update();
}

void vk::Renderer::glfwHandleKeyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
	assert(camera);

	const bool isReleased = (GLFW_RELEASE == action); // check if key is released -> if not, its being held down

	switch (key)
	{
		case GLFW_KEY_W:
			camera->inputMap[std::size_t(EInputState::FORWARD)] = !isReleased;
			break;

		case GLFW_KEY_S:
			camera->inputMap[std::size_t(EInputState::BACKWARD)] = !isReleased;
			break;

		case GLFW_KEY_A:
			camera->inputMap[std::size_t(EInputState::LEFT)] = !isReleased;
			break;

		case GLFW_KEY_D:
			camera->inputMap[std::size_t(EInputState::RIGHT)] = !isReleased;
			break;

		case GLFW_KEY_Q:
			camera->inputMap[std::size_t(EInputState::DOWN)] = !isReleased;
			break;
		case GLFW_KEY_E:
			camera->inputMap[std::size_t(EInputState::UP)] = !isReleased;
			break;

		case GLFW_KEY_LEFT_SHIFT:
			camera->inputMap[std::size_t(EInputState::FAST)] = !isReleased;
			break;
		case GLFW_KEY_LEFT_CONTROL:
			camera->inputMap[std::size_t(EInputState::SLOW)] = !isReleased;
			break;
		default:
			;
	}

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	// Default
	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
	{
		setRenderingPipeline = 1;
		setAlphaMakingPipeline = 5;
	}

	// Mip-map visual
	else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
	{
		setRenderingPipeline = 2;
	}

	// Partial Derivative visual
	else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
	{
		setRenderingPipeline = 3;
	}

	// Linear depth visual
	else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
	{
		setRenderingPipeline = 4;
	}

	// Key 5 is used for post-processing
	else if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS)
	{
		setRenderingPipeline = 6;
	}

	else if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS)
	{
		setRenderingPipeline = 7;
	}

	if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
	{
		postProcessSettings.Enable = postProcessSettings.Enable == true ? false : true;
		const std::string result = postProcessSettings.Enable == true ? "Enabled" : "Disabled";
		std::cout << "Post process: " << result << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS)
	{
		//setRenderingPipeline = 8;
		vk::renderType = RenderType::MESH_DENSITY;
	}

	// Select render-type
	if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS)
	{
		vk::renderType = RenderType::FORWARD;
	}

	if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
	{
		vk::renderType = RenderType::DEFERRED;
	}
}

void vk::Renderer::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
	assert(camera);

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		auto& flag = camera->inputMap[std::size_t(EInputState::MOUSING)];

		flag = !flag; // we're using mouse now
		if (flag)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
}

// Get the current mouse position
void vk::Renderer::glfwCallbackMotion(GLFWwindow* window, double x, double y)
{
	auto camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
	assert(camera);

	camera->mouseX = float(x);
	camera->mouseY = float(y);
}
