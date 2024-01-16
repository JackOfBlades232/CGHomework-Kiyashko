#include "simple_compute.h"

#include <etna/Etna.hpp>


SimpleCompute::SimpleCompute(uint32_t a_length) : m_length(a_length) {}

void SimpleCompute::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  for (uint32_t i = 0; i < a_instanceExtensionsCount; ++i) {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }

  #ifndef NDEBUG
    m_instanceExtensions.push_back("VK_EXT_debug_report");
  #endif
  
  etna::initialize(etna::InitParams
    {
      .applicationName = "ComputeSample",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = m_instanceExtensions,
      .deviceExtensions = m_deviceExtensions,
      // .physicalDeviceIndexOverride = 0,
    }
  );

  m_context = &etna::get_context();
  m_cmdBufferCompute = m_context->createCommandBuffer();
}

void SimpleCompute::Cleanup()
{
  CleanupPipeline();
}
