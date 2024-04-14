#include "shadowmap_render.h"

#include <array>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateAAResources()
{
  const vk::Extent3D ssaaRtExtent{vk::Extent3D{m_width*2, m_height*2, 1}};

  ssaaRt = RenderTarget(etna::Image::CreateInfo
    {
      .extent     = ssaaRtExtent,
      .name       = "ssaa_rt",
      .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
    },
    m_context);
  ssaaDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = ssaaRtExtent,
    .name       = "ssaa_depth",
    .format     = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });
  ssaaGbuffer.normals = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = ssaaRtExtent,
    .name       = "ssaa_gbuf_normals",
    .format     = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });
  ssaaGbuffer.depth = &ssaaDepth;

  const vk::Extent3D msaaRtExtent{vk::Extent3D{m_width, m_height, 1}};

  msaaRt = RenderTarget(etna::Image::CreateInfo
    {
      .extent     = msaaRtExtent,
      .name       = "msaa_rt",
      .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
      .samples    = vk::SampleCountFlagBits::e4
    },
    m_context);
  msaaDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = msaaRtExtent,
    .name       = "msaa_depth",
    .format     = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
    .samples    = vk::SampleCountFlagBits::e4
  });

  const vk::Extent3D taaRtExtent{vk::Extent3D{m_width, m_height, 1}};

  taaFrames[0] = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = taaRtExtent,
    .name       = "taa_rt",
    .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc
  });
  taaFrames[1] = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = taaRtExtent,
    .name       = "taa_prev_rt",
    .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc
  });
}

void SimpleShadowmapRender::DeallocateAAResources()
{
  ssaaRt = std::move(RenderTarget());
  ssaaDepth.reset();
  ssaaGbuffer.normals.reset();

  msaaRt = std::move(RenderTarget());
  msaaDepth.reset();

  taaFrames[0].reset();
  taaFrames[1].reset();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::SetupAAPipelines()
{
  m_pTaaReprojector = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName    = "taa_simple_reprojection",
      .fragShaderPath = VK_GRAPHICS_BASIC_ROOT"/resources/shaders/taa_simple.frag.spv",
      .format         = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .extent         = vk::Extent2D{m_width, m_height}
    });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordAAResolveCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  switch (currentAATechnique)
  {
  case eSsaa:
  {
    BlitRTToScreen(
      a_cmdBuff, ssaaRt.current(), 
      vk::Extent2D{m_width*2, m_height*2}, VK_FILTER_LINEAR, 
      a_targetImage, a_targetImageView);
  } break;

  case eMsaa:
  { 
    etna::set_state(a_cmdBuff, msaaRt.current().get(), 
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

    VkImageResolve resolve;
    resolve.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    resolve.srcSubresource.mipLevel       = 0;
    resolve.srcSubresource.baseArrayLayer = 0;
    resolve.srcSubresource.layerCount     = 1;
    resolve.srcOffset                     = { 0, 0, 0 };
    resolve.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    resolve.dstSubresource.mipLevel       = 0;
    resolve.dstSubresource.baseArrayLayer = 0;
    resolve.dstSubresource.layerCount     = 1;
    resolve.dstOffset                     = { 0, 0, 0 };
    resolve.extent                        = { m_width, m_height, 1 };

    vkCmdResolveImage(
      a_cmdBuff,
      msaaRt.current().get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      a_targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &resolve);
    
    // Separate from present set_states for debug ui drawing
    etna::set_state(a_cmdBuff, a_targetImage, 
      vk::PipelineStageFlagBits2::eAllGraphics,
      vk::AccessFlags2(vk::AccessFlagBits2::eColorAttachmentWrite),
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageAspectFlagBits::eColor);
  } break;

  case eTaa:
  {
    m_pTaaReprojector->RecordCommands(a_cmdBuff, taaCurFrame->get(), taaCurFrame->getView({}), 
      {{
        etna::Binding{0, constants.genBinding()}, 
        etna::Binding{1, mainViewRt.current().genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, taaPrevFrame->genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{3, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
      }});

    BlitRTToScreen(
      a_cmdBuff, *taaCurFrame, 
      vk::Extent2D{m_width, m_height}, VK_FILTER_NEAREST,
      a_targetImage, a_targetImageView);
    
    resetReprojection = false;
    std::swap(taaCurFrame, taaPrevFrame);
  } break;
  }
}

float SimpleShadowmapRender::CurrentTaaReprojectionCoeff()
{
  return resetReprojection ? 0.0f : currentReprojectionCoeff;
}

