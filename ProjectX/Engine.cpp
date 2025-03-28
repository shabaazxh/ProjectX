#include "baked_model.hpp"
#include "Engine.hpp"
#include "Image.hpp"
#include <glm/glm.hpp>
#include "Utils.hpp"

vk::Engine::Engine()
{
	m_isRunning = false;
	m_lastFrameTime = 0.0;
}

bool vk::Engine::Initialize()
{
	std::cout << "=========================== CONTROLS ===========================================" << std::endl;
	std::cout << "** Right-Mouse to activate & deactivate Camera" << std::endl;
	std::cout << "** Camera - WSADQE " << std::endl;
	std::cout << "** Key 1: Default Render" << std::endl;
	std::cout << "** Key 2: Depth Visual" << std::endl;
	std::cout << "** Key 3: Mipmap Visual" << std::endl;
	std::cout << "** Key 4: Partial Derivative Visual" << std::endl;
	std::cout << "** Key 5: Mosiac Post-Processing" << std::endl;
	std::cout << "** Key 6: Overdraw visualization" << std::endl;
	std::cout << "** Key 7: Overshading visualization" << std::endl;
	std::cout << "** Key 8: Mesh Density visualization (Enable Forward renderer [key 9])" << std::endl;
	std::cout << "** Key 9: Forward Renderer " << std::endl;
	std::cout << "** Key 0: Deferred Renderer " << std::endl;
	std::cout << "================================================================================" << std::endl;

	if (m_context.MakeContext(1280, 720))
	{
		m_isRunning = true;
	}

	m_Renderer = std::make_unique<Renderer>(m_context);

	return m_isRunning;
}


void vk::Engine::Shutdown()
{
	m_Renderer->Destroy();
	m_Renderer.reset();
	m_context.Destroy(); // Free vulkan device, allocator, window
}

void vk::Engine::Run()
{
	while (m_isRunning && !glfwWindowShouldClose(m_context.window))
	{
		double currentFrameTime = glfwGetTime();
		deltaTime = currentFrameTime - m_lastFrameTime;
		m_lastFrameTime = currentFrameTime;

		glfwPollEvents();
		Update(deltaTime);
		Render();
	}

	Shutdown();
}

void vk::Engine::Update(double deltaTime)
{
	m_Renderer->Update(deltaTime);
}

void vk::Engine::Render()
{
	m_Renderer->Render();
}
