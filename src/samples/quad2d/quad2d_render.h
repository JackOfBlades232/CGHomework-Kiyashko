#ifndef SIMPLE_QUAD2D_RENDER_H
#define SIMPLE_QUAD2D_RENDER_H

#define VK_NO_PROTOTYPES
#include "../../render/render_common.h"
#include "../../render/quad_renderer.h"
#include "../resources/shaders/common.h"
#include <vk_swapchain.h>

#include <string>
#include <iostream>

#include <etna/GlobalContext.hpp>

class Quad2D_Render : public IRender
{
public:
  Quad2D_Render(uint32_t a_width, uint32_t a_height);
  ~Quad2D_Render();

  inline uint32_t     GetWidth()      const override { return m_width; }
  inline uint32_t     GetHeight()     const override { return m_height; }
  inline VkInstance   GetVkInstance() const override { return m_context->getInstance(); }
  void InitVulkan(const char **a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR &a_surface, bool initGUI) override;

  void ProcessInput(const AppInput &input) override;
  void UpdateCamera(const Camera *, uint32_t) override {}

  void LoadScene(const char *path, bool transpose_inst_matrices) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;

private:
  etna::GlobalContext *m_context = nullptr;
  std::unique_ptr<QuadRenderer> m_pQuad;

  etna::Image m_fullscreenImage;
  etna::Sampler m_defaultSampler;
  struct ImageData
  {
    uint32_t w, h;
    std::vector<uint32_t> content;
  } m_imageData{};

  vk::CommandPool m_commandPool = VK_NULL_HANDLE;

  struct
  {
    uint32_t      currentFrame      = 0u;
    vk::Queue     queue             = VK_NULL_HANDLE;
    vk::Semaphore imageAvailable    = VK_NULL_HANDLE;
    vk::Semaphore renderingFinished = VK_NULL_HANDLE;
  } m_presentationResources;

  std::vector<vk::Fence> m_frameFences;
  std::vector<vk::CommandBuffer> m_cmdBuffersDrawMain;

  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VulkanSwapChain m_swapchain;

  uint32_t m_width  = 1024u;
  uint32_t m_height = 1024u;
  uint32_t m_framesInFlight = 2u;
  bool m_vsync = false;

  vk::PhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char*> m_deviceExtensions;
  std::vector<const char*> m_instanceExtensions;

  void DrawFrameSimple();

  void BuildCommandBufferSimple(vk::CommandBuffer a_cmdBuff, vk::Image a_targetImage, vk::ImageView a_targetImageView);

  void RecreateSwapChain();

  void SetupDeviceExtensions();

  void AllocateResources();
  void DeallocateResources();
  
  void SetupQuadRenderer();

  void InitPresentStuff();
  void ResetPresentStuff();
};


#endif //SIMPLE_QUAD2D_RENDER_H
