#include "quad2d_render.h"
#include <etna/Vulkan.hpp>

void Quad2D_Render::InitPresentStuff()
{
  m_presentationResources.imageAvailable    = etna::unwrap_vk_result(m_context->getDevice().createSemaphore({vk::SemaphoreCreateInfo{}}));
  m_presentationResources.renderingFinished = etna::unwrap_vk_result(m_context->getDevice().createSemaphore({vk::SemaphoreCreateInfo{}}));

  // TODO: Move to customizable initialization
  m_commandPool = vk_utils::createCommandPool(m_context->getDevice(), m_context->getQueueFamilyIdx(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  // @TODO(PKiyashko): add command pool to etna context and move buffer creation there
  m_cmdBuffersDrawMain.reserve(m_framesInFlight);
  {
    std::vector<VkCommandBuffer> createdBuffers = vk_utils::createCommandBuffers(m_context->getDevice(), m_commandPool, m_framesInFlight);
    for (int i = 0; i < m_framesInFlight; i++)
      m_cmdBuffersDrawMain.push_back(static_cast<vk::CommandBuffer>(createdBuffers[i]));
  }

  m_frameFences.resize(m_framesInFlight);
  vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };
  for (size_t i = 0; i < m_framesInFlight; i++)
    m_frameFences[i] = etna::unwrap_vk_result(m_context->getDevice().createFence({fenceInfo}));
}

void Quad2D_Render::ResetPresentStuff()
{
  if (!m_cmdBuffersDrawMain.empty())
  {
    m_context->getDevice().freeCommandBuffers(m_commandPool, m_cmdBuffersDrawMain);
    m_cmdBuffersDrawMain.clear();
  }

  for (size_t i = 0; i < m_frameFences.size(); i++)
    m_context->getDevice().destroyFence(m_frameFences[i]);

  if (m_presentationResources.imageAvailable != VK_NULL_HANDLE)
    m_context->getDevice().destroySemaphore(m_presentationResources.imageAvailable);
  if (m_presentationResources.renderingFinished != VK_NULL_HANDLE)
    m_context->getDevice().destroySemaphore(m_presentationResources.renderingFinished);

  if (m_commandPool != VK_NULL_HANDLE)
    m_context->getDevice().destroyCommandPool(m_commandPool);
}

void Quad2D_Render::InitPresentation(VkSurfaceKHR &a_surface, bool)
{
  m_surface = a_surface;

  m_presentationResources.queue = m_swapchain.CreateSwapChain(
    m_context->getPhysicalDevice(), m_context->getDevice(), m_surface,
    m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  AllocateResources();
  InitPresentStuff();
}
