#include "Context.hpp"
#include "SSR.h"

vk::SSR::SSR(Context& context, const Image& inputImage, const Image& depthBuffer) : context{context}, inputImage{inputImage}, depthBuffer{depthBuffer}
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

}

vk::SSR::~SSR()
{

}

void vk::SSR::Resize()
{

}

void vk::SSR::Execute(VkCommandBuffer cmd)
{
}

void vk::SSR::CreatePipeline()
{
}
void vk::SSR::CreateRenderPass()
{
}

void vk::SSR::CreateFramebuffer()
{
}

void vk::SSR::BuildDescriptors()
{
}


