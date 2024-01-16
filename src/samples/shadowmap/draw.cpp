#include "shadowmap_render.h"
#include <etna/Etna.hpp>

#include <imgui/imgui.h>

#include "../../render/render_gui.h"

void SimpleShadowmapRender::DrawFrameSimple(bool draw_gui)
{
  vk::Device device = m_context->getDevice();
  device.waitForFences({ m_frameFences[m_presentationResources.currentFrame] }, VK_TRUE, UINT64_MAX);
  device.resetFences({ m_frameFences[m_presentationResources.currentFrame] });

  uint32_t imageIdx;
  m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);

  auto currentCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];

  vk::Semaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

  BuildCommandBufferSimple(currentCmdBuf, m_swapchain.GetAttachment(imageIdx).image, m_swapchain.GetAttachment(imageIdx).view);

  std::vector<vk::CommandBuffer> submitCmdBufs = { currentCmdBuf };

  if (draw_gui)
  {
    ImDrawData* pDrawData = ImGui::GetDrawData();
    auto currentGUICmdBuf = m_pGUIRender->BuildGUIRenderCommand(imageIdx, pDrawData);
    submitCmdBufs.push_back(currentGUICmdBuf);
  }

  vk::Semaphore signalSemaphores[] = {m_presentationResources.renderingFinished};
  vk::SubmitInfo submitInfo = 
  {
    .waitSemaphoreCount   = 1,
    .pWaitSemaphores      = waitSemaphores,
    .pWaitDstStageMask    = waitStages,
    .commandBufferCount   = (uint32_t)submitCmdBufs.size(),
    .pCommandBuffers      = submitCmdBufs.data(),
    .signalSemaphoreCount = 1,
    .pSignalSemaphores    = signalSemaphores
  };

  vk::Queue queue = m_context->getQueue();
  // @TODO: check result hpp-style
  queue.submit({ submitInfo });

  // @TODO: all the rest shall be remade
  vk::Result presentRes = (vk::Result)m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
                                                               m_presentationResources.renderingFinished);

  if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != vk::Result::eSuccess)
  {
    ETNA_PANIC("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  m_presentationResources.queue.waitIdle();
  etna::submit();
}

void SimpleShadowmapRender::DrawFrame(float a_time, DrawMode a_mode)
{
  UpdateUniformBuffer(a_time);
  switch (a_mode)
  {
    case DrawMode::WITH_GUI:
      SetupGUIElements();
      DrawFrameSimple(true);
      break;
    case DrawMode::NO_GUI:
      DrawFrameSimple(false);
      break;
    default:
      DrawFrameSimple(false);
  }

}
