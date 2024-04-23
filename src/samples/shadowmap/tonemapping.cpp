#include "shadowmap_render.h"

void SimpleShadowmapRender::SetupTonemappingPipelines()
{
  m_pToneMapper = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName    = "tonemapping",
      .fragShaderPath = VK_GRAPHICS_BASIC_ROOT"/resources/shaders/tonemapping.frag.spv",
      .format         = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .extent         = vk::Extent2D{m_width, m_height}
    });
}

void SimpleShadowmapRender::RecordTonemappingCommands(
    VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  m_pToneMapper->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, 
    {{ 
      etna::Binding{0, constants.genBinding()},
      etna::Binding{1, mainRt.current().genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    }});
}
