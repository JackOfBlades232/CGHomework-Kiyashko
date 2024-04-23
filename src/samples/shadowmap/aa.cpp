#include "shadowmap_render.h"

#include <array>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateAAResources()
{
  ssaaGbuffer = GBuffer(vk::Extent2D{m_width * 2, m_height * 2}, m_context, "ssaa");
  ssaaFrame = m_context->createImage(etna::Image::CreateInfo
    {
      .extent     = vk::Extent3D{m_width*2, m_height*2, 1},
      .name       = "ssaa_rt",
      .format     = vk::Format::eR16G16B16A16Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc
    });

  const vk::Extent3D taaRtExtent{vk::Extent3D{m_width, m_height, 1}};
  taaFrames[0] = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = taaRtExtent,
    .name       = "taa_rt",
    .format     = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc
  });
  taaFrames[1] = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = taaRtExtent,
    .name       = "taa_prev_rt",
    .format     = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc
  });
}

void SimpleShadowmapRender::DeallocateAAResources()
{
  ssaaFrame.reset();
  ssaaGbuffer.reset();

  taaFrames[0].reset();
  taaFrames[1].reset();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::SetupAAPipelines()
{
  m_pTaaReprojector = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName    = "taa_simple_reprojection",
      .fragShaderPath = VK_GRAPHICS_BASIC_ROOT"/resources/shaders/taa_simple.frag.spv",
      .format         = vk::Format::eR16G16B16A16Sfloat,
      .extent         = vk::Extent2D{m_width, m_height}
    });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordAAResolveCommands(VkCommandBuffer a_cmdBuff)
{
  switch (currentAATechnique)
  {
  case eSsaa:
  {
    BlitToTarget(
      a_cmdBuff, mainRt.current().get(), mainRt.current().getView({}),
      ssaaFrame, vk::Extent2D{m_width*2, m_height*2}, VK_FILTER_LINEAR);
  } break;

  case eTaa:
  {
    m_pTaaReprojector->RecordCommands(a_cmdBuff, taaCurFrame->get(), taaCurFrame->getView({}), 
      {{
        etna::Binding{0, constants.genBinding()}, 
        etna::Binding{1, mainRt.current().genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, taaPrevFrame->genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{3, mainGbuffer.depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
      }});

    BlitToTarget(
      a_cmdBuff, mainRt.current().get(), mainRt.current().getView({}),
      *taaCurFrame, vk::Extent2D{m_width, m_height}, VK_FILTER_NEAREST);
    
    resetReprojection = false;
    std::swap(taaCurFrame, taaPrevFrame);
  } break;
  }
}

float SimpleShadowmapRender::CurrentTaaReprojectionCoeff()
{
  return resetReprojection ? 0.0f : currentReprojectionCoeff;
}

