#include "Context.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "RenderPass.hpp"
#include "ImGuiRenderer.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "Utils.hpp"


void vk::ImGuiRenderer::Initialize(const Context& context) {

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint64_t>(ImGuiPoolSizes.size());
    pool_info.pPoolSizes = ImGuiPoolSizes.data();

    // TODO: @DEBUG: This should use the engines error logger to make the user aware if this fails
    VK_CHECK(vkCreateDescriptorPool(context.device, &pool_info, nullptr, &ImGuiRenderer::imGuiDescriptorPool), "Failed to create ImGui descriptor pool.");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO io = ImGui::GetIO();
    (void)io;

    ImGui_ImplGlfw_InitForVulkan(context.window, true);

    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = context.instance;
    info.PhysicalDevice = context.pDevice;
    info.Device = context.device;
    info.Queue = context.graphicsQueue;
    info.QueueFamily = context.graphicsFamilyIndex;
    info.DescriptorPool = ImGuiRenderer::imGuiDescriptorPool;
    info.MinImageCount = 3;
    info.ImageCount = MAX_FRAMES_IN_FLIGHT;
    info.Subpass = 0;
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.RenderPass = context.renderPass;

    ImGui_ImplVulkan_Init(&info);

    io.Fonts->AddFontDefault();
}

void vk::ImGuiRenderer::Update(const std::shared_ptr<Scene>& scene, const std::shared_ptr<Camera>& camera)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::BeginChild("Settings");
    // Display FPS
    ImGui::TextColored(
        ImVec4(0.76, 0.5, 0.0, 1.0), "FPS: (%.1f FPS), %.3f ms/frame",
        ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

    // Add camera position
    ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
        camera->GetPosition().x,
        camera->GetPosition().y,
        camera->GetPosition().z
    );

    ImGui::Text("Directional Light: (%.2f, %.2f, %.2f)",
        scene->GetLights()[0].position.x,
        scene->GetLights()[0].position.y,
        scene->GetLights()[0].position.z
    );

    static bool initialized = false;
    static float SunElevation = 0.0f;
    static float SunAzimuthal = 0.0f;
    static const float distance = 1.0f;

    if (!initialized) {
        SunElevation = -2.7f; // default elevation
        SunAzimuthal = -90.0f; // default azimuth
        initialized = true;
    }

    // Phis is elevation
    // Theta is azimuthal
    const float ElevationPhi = glm::radians(SunElevation);
    const float AzimuthalTheta = glm::radians(SunAzimuthal);

    const float x = cosf(ElevationPhi) * cosf(AzimuthalTheta) * distance;
    const float z = cosf(ElevationPhi) * sinf(AzimuthalTheta) * distance;
    const float y = sinf(ElevationPhi) * distance;

    auto& lights = scene->GetLights();
    if (lights.empty()) return;

    auto& sunLight = lights[0];

    sunLight.position.x = x;
    sunLight.position.y = y;
    sunLight.position.z = z;
    sunLight.position = glm::normalize(sunLight.position);

    if (ImGui::CollapsingHeader("Directional Light"))
    {
        ImGui::Text("Sun Angles");
        ImGui::SliderFloat("Elevation - Phi", &SunElevation, -90.0f, 90.0f, "%.2f");
        ImGui::SliderFloat("Azimuthal - Theta", &SunAzimuthal, -90.0f, 90.0f, "%.2f");

        ImGui::Text("Light Camera Settings");
        ImGui::SliderFloat("View", &sunLight.View, -200.0f, 200.0f, "%.2f");
        ImGui::SliderFloat("Near", &sunLight.Near, 0.1f, 100.0f, "%.2f");
        ImGui::SliderFloat("Far", &sunLight.Far, 0.1f, 1000.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Lights")) {
        auto& lights = scene->GetLights();
        for (size_t i = 1; i < lights.size() - 1; ++i) {
            if (lights[i].Type != LightType::Directional) {
                std::string label = "Light " + std::to_string(i) + " Position";
                ImGui::SliderFloat3(label.c_str(), &lights[i].position.x, -600.0f, 600.0f, "%.2f");
            }
        }
    }

    static bool enableTextureDebug = false;
    ImGui::Checkbox("Debug Textures", &enableTextureDebug);
    if (enableTextureDebug)
    {
        ImGui::Begin("Debug Textures");
        for (size_t i = 0; i < textureIDs.size(); ++i) {
            ImGui::Image(textureIDs[i], ImVec2(400, 400));
        }
        ImGui::End();
    }


    ImGui::EndChild();
}

void vk::ImGuiRenderer::Render(VkCommandBuffer cmd, const Context& context, uint32_t imageIndex)
{
    ImGui::Render();
    ImDrawData* main_draw_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(main_draw_data, cmd);
}

void vk::ImGuiRenderer::Shutdown(const Context& context)
{
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context.device, imGuiDescriptorPool, nullptr);
    vkDestroyRenderPass(context.device, imGuiRenderPass, nullptr);

    ImGui_ImplGlfw_Shutdown();
}

void vk::ImGuiRenderer::AddTexture(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
    ImTextureID textureID = ImGui_ImplVulkan_AddTexture(sampler, imageView, imageLayout);
    textureIDs.push_back(static_cast<void*>(textureID));
}
