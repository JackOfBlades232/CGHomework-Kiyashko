#include "shadowmap_render.h"

#include <array>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateAAResources()
{
  const vk::Extent3D aaRtExtent{vk::Extent3D{m_width*currentAAScale, m_height*currentAAScale, 1}};

  ssaaRt = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = aaRtExtent,
    .name = "ssaa_rt",
    .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
  });
  ssaaDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = aaRtExtent,
    .name = "ssaa_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });
}

void SimpleShadowmapRender::DeallocateAAResources()
{
  ssaaRt.reset();
  ssaaDepth.reset();
}

void SimpleShadowmapRender::RecreateAATexOnScaleChange()
{
  DeallocateAAResources();
  AllocateAAResources();
}


/// TECHNIQUE CHOICE

etna::Image *SimpleShadowmapRender::CurrentAARenderTarget()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return &ssaaRt;
  case eMsaa:
  case eNone:
    return nullptr;
  }
}

etna::Image *SimpleShadowmapRender::CurrentAADepthTex()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return &ssaaDepth;
  case eMsaa:
  case eNone:
    return nullptr;
  }
}

vk::Rect2D SimpleShadowmapRender::CurrentAARect()
{
  return vk::Rect2D{0, 0, m_width*currentAAScale, m_height*currentAAScale};
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordAAResolveCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  switch (currentAATechnique)
  {
  case eSsaa:
  {
    etna::set_state(a_cmdBuff, ssaaRt.get(), 
      vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlags2(vk::AccessFlagBits2::eTransferRead),
      vk::ImageLayout::eTransferSrcOptimal,
      vk::ImageAspectFlagBits::eColor);
    etna::set_state(a_cmdBuff, a_targetImage, 
      vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlags2(vk::AccessFlagBits2::eTransferWrite),
      vk::ImageLayout::eTransferDstOptimal,
      vk::ImageAspectFlagBits::eColor);
    etna::flush_barriers(a_cmdBuff);

    VkImageBlit blit;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {(int32_t)m_width*currentAAScale, (int32_t)m_width*currentAAScale, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {(int32_t)m_width, (int32_t)m_width, 1};

    vkCmdBlitImage(
      a_cmdBuff,
      ssaaRt.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      a_targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &blit,
      VK_FILTER_LINEAR);
    
    // Separate from present set_states for debug ui drawing
    etna::set_state(a_cmdBuff, a_targetImage, 
      vk::PipelineStageFlagBits2::eAllGraphics,
      vk::AccessFlags2(vk::AccessFlagBits2::eColorAttachmentWrite),
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageAspectFlagBits::eColor);
    etna::flush_barriers(a_cmdBuff);

  } break;

  case eMsaa:
  {

  } break;
  }
}
