#include "shadowmap_render.h"

#include "../../render/render_gui.h"

void SimpleShadowmapRender::InitPresentStuff()
{
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_presentationResources.imageAvailable));
  VK_CHECK_RESULT(vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_presentationResources.renderingFinished));

  m_cmdBuffersDrawMain.reserve(m_framesInFlight);
  m_cmdBuffersDrawMain = createCommandBuffers(m_framesInFlight);

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_context->getDevice(), &fenceInfo, nullptr, &m_frameFences[i]));
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
    freeCommandBuffers(m_cmdBuffersDrawMain);
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

void SimpleShadowmapRender::InitPresentation(VkSurfaceKHR &a_surface, bool)
{
  m_surface = a_surface;

  m_presentationResources.queue = m_swapchain.CreateSwapChain(
    m_context->getPhysicalDevice(), m_context->getDevice(), m_surface,
    m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  AllocateResources();
  InitPresentStuff();
}

std::vector<VkCommandBuffer> SimpleShadowmapRender::createCommandBuffers(uint32_t cnt)
{
  std::vector<vk::CommandBuffer> buffers = m_context->createCommandBuffers(cnt);
  std::vector<VkCommandBuffer> result(buffers.size());
  for (size_t i = 0; i < result.size(); i++)
    result[i] = buffers[i];
  return result;
}

void SimpleShadowmapRender::freeCommandBuffers(std::vector<VkCommandBuffer> &buffers)
{
  std::vector<vk::CommandBuffer> vkBuffers(buffers.size());
  for (size_t i = 0; i < vkBuffers.size(); i++)
    vkBuffers[i] = buffers[i];
  m_context->freeCommandBuffers(vkBuffers);
}
