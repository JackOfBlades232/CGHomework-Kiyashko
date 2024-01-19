#include "shadowmap_render.h"
#include <etna/Assert.hpp>

#include "../../render/render_gui.h"

void SimpleShadowmapRender::InitPresentStuff()
{
  vk::SemaphoreCreateInfo semaphoreInfo = {};

  vk::Device device = m_context->getDevice();
  m_presentationResources.imageAvailable    = etna::validate_vk_result(device.createSemaphore({}));
  m_presentationResources.renderingFinished = etna::validate_vk_result(device.createSemaphore({}));

  m_cmdBuffersDrawMain = m_context->createCommandBuffers(m_framesInFlight);

  m_frameFences.resize(m_framesInFlight);
  vk::FenceCreateInfo fenceInfo = { .flags = vk::FenceCreateFlagBits::eSignaled };
  for (size_t i = 0; i < m_framesInFlight; i++)
    m_frameFences[i] = etna::validate_vk_result(device.createFence(fenceInfo));

  m_pGUIRender = std::make_shared<ImGuiRender>(
    m_context->getInstance(),
    m_context->getDevice(),
    m_context->getPhysicalDevice(),
    m_context->getQueueFamilyIdx(),
    m_context->getQueue(),
    m_swapchain);
}

void SimpleShadowmapRender::ResetPresentStuff()
{
  if (!m_cmdBuffersDrawMain.empty())
  {
    m_context->freeCommandBuffers(m_cmdBuffersDrawMain);
    m_cmdBuffersDrawMain.clear();
  }

  vk::Device device = m_context->getDevice();

  for (size_t i = 0; i < m_frameFences.size(); i++)
    device.destroyFence(m_frameFences[i]);

  if (m_presentationResources.imageAvailable != VK_NULL_HANDLE)
    device.destroySemaphore(m_presentationResources.imageAvailable);
  if (m_presentationResources.renderingFinished != VK_NULL_HANDLE)
    device.destroySemaphore(m_presentationResources.renderingFinished);
}

void SimpleShadowmapRender::InitPresentation(vk::SurfaceKHR &a_surface, bool)
{
  m_surface = a_surface;

  // @TODO: remake in the future
  m_presentationResources.queue = m_swapchain.CreateSwapChain(
    m_context->getPhysicalDevice(), m_context->getDevice(), *(VkSurfaceKHR *)&m_surface,
    m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  AllocateResources();
  InitPresentStuff();
}
