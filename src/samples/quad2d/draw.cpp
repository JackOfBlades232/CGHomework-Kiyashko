#include "quad2d_render.h"
#include <etna/Etna.hpp>

void Quad2D_Render::DrawFrameSimple()
{
  m_context->getDevice().waitForFences({m_frameFences[m_presentationResources.currentFrame]}, VK_TRUE, UINT64_MAX);
  m_context->getDevice().resetFences({m_frameFences[m_presentationResources.currentFrame]});

  uint32_t imageIdx;
  m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);

  vk::CommandBuffer currentCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];

  vk::Semaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

  BuildCommandBufferSimple(currentCmdBuf, m_swapchain.GetAttachment(imageIdx).image, m_swapchain.GetAttachment(imageIdx).view);

  vk::SubmitInfo submitInfo{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &currentCmdBuf,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &m_presentationResources.renderingFinished,
  };
  
  ETNA_CHECK_VK_RESULT(m_context->getQueue().submit({submitInfo}, {m_frameFences[m_presentationResources.currentFrame]}));

  vk::Result presentRes = static_cast<vk::Result>(
    m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx, m_presentationResources.renderingFinished)
  );

  if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != vk::Result::eSuccess)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;
  m_presentationResources.queue.waitIdle();
  
  etna::submit();
}

void Quad2D_Render::DrawFrame(float, DrawMode)
{
  DrawFrameSimple();
}
