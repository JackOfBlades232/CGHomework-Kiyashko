#include "shadowmap_render.h"

#include <array>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateAAResources()
{
  const vk::Extent3D ssaaRtExtent{vk::Extent3D{m_width*2, m_height*2, 1}};

  ssaaRt = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = ssaaRtExtent,
    .name       = "ssaa_rt",
    .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
  });
  ssaaDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = ssaaRtExtent,
    .name       = "ssaa_depth",
    .format     = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  const vk::Extent3D msaaRtExtent{vk::Extent3D{m_width, m_height, 1}};

  msaaRt = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = msaaRtExtent,
    .name       = "msaa_rt",
    .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    .samples    = vk::SampleCountFlagBits::e4
  });
  msaaDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = msaaRtExtent,
    .name       = "msaa_depth",
    .format     = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
    .samples    = vk::SampleCountFlagBits::e4
  });

  const vk::Extent3D taaRtExtent{vk::Extent3D{m_width, m_height, 1}};

  taaRt = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = taaRtExtent,
    .name       = "taa_rt",
    .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });
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
  ssaaRt.reset();
  ssaaDepth.reset();

  msaaRt.reset();
  msaaDepth.reset();

  taaRt.reset();
  taaFrames[0].reset();
  taaFrames[1].reset();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::LoadAAShaders()
{
  etna::create_program("taa_reprojection",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/taa_simple.frag.spv"});
}

void SimpleShadowmapRender::SetupAAPipelines()
{
  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_taaReprojectionPipeline = pipelineManager.createGraphicsPipeline("taa_reprojection",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
}


/// TECHNIQUE CHOICE

etna::Image *SimpleShadowmapRender::CurrentAARenderTarget()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return &ssaaRt;
  case eMsaa:
    return &msaaRt;
  case eTaa:
    return &taaRt;
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
    return &msaaDepth;
  case eTaa: // @TODO(PKiyashko): this is janky and untrue. Return mainDepthTex? But then it won't be aa specific
  case eNone:
    return nullptr;
  }
}

vk::Rect2D SimpleShadowmapRender::CurrentAARect()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return vk::Rect2D{0, 0, m_width*2, m_height*2};
  case eMsaa:
  case eTaa:
  case eNone:
    return vk::Rect2D{0, 0, m_width, m_height};
  }
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordAAResolveCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  switch (currentAATechnique)
  {
  case eSsaa:
  {
    // @TODO(PKiyashko): pull blits out as utility functions.
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
    blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel       = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount     = 1;
    blit.srcOffsets[0]                 = { 0, 0, 0 };
    blit.srcOffsets[1]                 = { (int32_t)m_width * 2, (int32_t)m_width * 2, 1 };
    blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel       = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount     = 1;
    blit.dstOffsets[0]                 = { 0, 0, 0 };
    blit.dstOffsets[1]                 = { (int32_t)m_width, (int32_t)m_width, 1 };

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
  } break;

  case eMsaa:
  { 
    etna::set_state(a_cmdBuff, msaaRt.get(), 
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
      msaaRt.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
    {
      auto programInfo = etna::get_shader_program("taa_reprojection");
      auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(0), a_cmdBuff,
        {
          etna::Binding {0, constants.genBinding()}, 
          etna::Binding {1, taaRt.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {2, taaPrevFrame->genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {3, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
        });
      VkDescriptorSet vkSet = set.getVkSet();

      etna::RenderTargetState renderTargets(a_cmdBuff, vk::Rect2D{0, 0, m_width, m_height}, 
        {{.image = taaCurFrame->get(), .view = taaCurFrame->getView({})}}, {});

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_taaReprojectionPipeline.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_taaReprojectionPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

      vkCmdDraw(a_cmdBuff, 3, 1, 0, 0);
    }

    etna::set_state(a_cmdBuff, taaCurFrame->get(), 
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
    blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel       = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount     = 1;
    blit.srcOffsets[0]                 = { 0, 0, 0 };
    blit.srcOffsets[1]                 = { (int32_t)m_width, (int32_t)m_width, 1 };
    blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel       = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount     = 1;
    blit.dstOffsets[0]                 = { 0, 0, 0 };
    blit.dstOffsets[1]                 = { (int32_t)m_width, (int32_t)m_width, 1 };

    vkCmdBlitImage(
      a_cmdBuff,
      taaCurFrame->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      a_targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &blit,
      VK_FILTER_NEAREST);
    
    // Separate from present set_states for debug ui drawing
    etna::set_state(a_cmdBuff, a_targetImage, 
      vk::PipelineStageFlagBits2::eAllGraphics,
      vk::AccessFlags2(vk::AccessFlagBits2::eColorAttachmentWrite),
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageAspectFlagBits::eColor);
    
    resetReprojection = false;
    std::swap(taaCurFrame, taaPrevFrame);
  } break;
  }
}

float SimpleShadowmapRender::CurrentTaaReprojectionCoeff()
{
  return resetReprojection ? 0.0f : currentReprojectionCoeff;
}

