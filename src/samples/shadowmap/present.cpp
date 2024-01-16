#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::InitPresentStuff()
{
  vk::SemaphoreCreateInfo semaphoreInfo = {};

  vk::Device device = m_context->getDevice();
  // @TODO: error checks
  m_presentationResources.imageAvailable = device.createSemaphore({}).value;
  m_presentationResources.renderingFinished = device.createSemaphore({}).value;

  m_cmdBuffersDrawMain = m_context->createCommandBuffers(m_framesInFlight);

  m_frameFences.resize(m_framesInFlight);
  vk::FenceCreateInfo fenceInfo = { .flags = vk::FenceCreateFlagBits::eSignaled };
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    // @TODO: error checks
    m_frameFences[i] = device.createFence(fenceInfo).value;
  }

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

  for (size_t i = 0; i < m_frameFences.size(); i++)
  {
    vkDestroyFence(m_context->getDevice(), m_frameFences[i], nullptr);
  }

  if (m_presentationResources.imageAvailable != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_context->getDevice(), m_presentationResources.imageAvailable, nullptr);
  }
  if (m_presentationResources.renderingFinished != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_context->getDevice(), m_presentationResources.renderingFinished, nullptr);
  }
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
