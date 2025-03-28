#include "Context.hpp"
#include "SSR.h"
#include "Utils.hpp"
#include "Pipeline.hpp"
#include "RenderPass.hpp"

vk::SSR::SSR(Context& context,
    const Image& inputImage,
    const Image& depthBuffer,
    const Image& metallicRoughness,
    const Image& normalsTexture,
    std::shared_ptr<Camera> camera) :
    context{ context },
    inputImage{ inputImage },
    depthBuffer{ depthBuffer },
    metallicRoughness{ metallicRoughness },
    normalsTexture{normalsTexture},
    camera { camera }
{
    m_width = context.extent.width;
    m_height = context.extent.height;

    m_RenderTarget = CreateImageTexture2D(
        "SSR_RenderTarget",
        context,
        m_width,
        m_height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1
    );

    m_SSRUniform.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto& buffer : m_SSRUniform)
    {
        buffer = CreateBuffer("SSRSettingsUBO", context, sizeof(SSRSettings), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    BuildDescriptors();
    CreateRenderPass();
    CreateFramebuffer();
    CreatePipeline();
}

void vk::SSR::Update()
{
    m_SSRUniform[currentFrame].WriteToBuffer(ssrSettings, sizeof(SSRSettings));
}

vk::SSR::~SSR()
{
    for (auto& buffer : m_SSRUniform)
    {
        buffer.Destroy(context.device);
    }
    m_RenderTarget.Destroy(context.device);
    vkDestroyPipeline(context.device, m_Pipeline, nullptr);
    vkDestroyPipelineLayout(context.device, m_PipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, m_DescriptorSetLayout, nullptr);
    vkDestroyFramebuffer(context.device, m_Framebuffer, nullptr);
    vkDestroyRenderPass(context.device, m_RenderPass, nullptr);
}

void vk::SSR::Resize()
{

}

void vk::SSR::Execute(VkCommandBuffer cmd)
{
#ifdef _DEBUG
    RenderPassLabel(cmd, "SSR");
#endif // !DEBUG

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_Framebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { m_width, m_height };

    std::array<VkClearValue, 1> clearValues{};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, m_DescriptorSets.data(), 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

#ifdef _DEBUG
    EndRenderPassLabel(cmd);
#endif // !DEBUG
}

void vk::SSR::CreatePipeline()
{
    auto pipelineResult = vk::PipelineBuilder(context.device, PipelineType::GRAPHICS, VertexBinding::NONE, 0)
        .AddShader("../Engine/assets/shaders/fs_tri.vert.spv", ShaderType::VERTEX)
        .AddShader("../Engine/assets/shaders/SSR.frag.spv", ShaderType::FRAGMENT)
        .SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetDynamicState({ {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR} })
        .SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetPipelineLayout({ {m_DescriptorSetLayout} })
        .SetSampling(VK_SAMPLE_COUNT_1_BIT)
        .AddBlendAttachmentState()
        .SetDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
        .SetRenderPass(m_RenderPass)
        .Build();

    m_Pipeline = pipelineResult.first;
    m_PipelineLayout = pipelineResult.second;
}
void vk::SSR::CreateRenderPass()
{
    RenderPass builder(context.device, 1);

    m_RenderPass = builder
        .AddAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .AddColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        // External -> 0 : Color
        .AddDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT)

        // 0 -> External : Color : Wait for color writing to finish on the attachment before the fragment shader tries to read from it
        .AddDependency(0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT)
        .Build();

    context.SetObjectName(context.device, (uint64_t)m_RenderPass, VK_OBJECT_TYPE_RENDER_PASS, "SSRRenderPass");
}

void vk::SSR::CreateFramebuffer()
{
    std::vector<VkImageView> attachments = { m_RenderTarget.imageView };

    VkFramebufferCreateInfo fbcInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_RenderPass,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = m_width,
        .height = m_height,
        .layers = 1
    };

    VK_CHECK(vkCreateFramebuffer(context.device, &fbcInfo, nullptr, &m_Framebuffer), "Failed to create SSR framebuffer.");
}

void vk::SSR::BuildDescriptors()
{
    m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            CreateDescriptorBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
            CreateDescriptorBinding(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
            CreateDescriptorBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
            CreateDescriptorBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
            CreateDescriptorBinding(4, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
            CreateDescriptorBinding(5, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        m_DescriptorSetLayout = CreateDescriptorSetLayout(context, bindings);
    }

    AllocateDescriptorSets(context, context.descriptorPool, m_DescriptorSetLayout, MAX_FRAMES_IN_FLIGHT, m_DescriptorSets);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = camera->GetBuffers()[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraTransform);
        UpdateDescriptorSet(context, 0, bufferInfo, m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_SSRUniform[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SSRSettings);
        UpdateDescriptorSet(context, 1, bufferInfo, m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo = {
            .sampler = clampToEdgeSamplerAniso,
            .imageView = depthBuffer.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };

        UpdateDescriptorSet(context, 2, imageInfo, m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo = {
            .sampler = clampToEdgeSamplerAniso,
            .imageView = inputImage.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        UpdateDescriptorSet(context, 3, imageInfo, m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo = {
            .sampler = clampToEdgeSamplerAniso,
            .imageView = metallicRoughness.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        UpdateDescriptorSet(context, 4, imageInfo, m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo = {
            .sampler = clampToEdgeSamplerAniso,
            .imageView = normalsTexture.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        UpdateDescriptorSet(context, 5, imageInfo, m_DescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
}


