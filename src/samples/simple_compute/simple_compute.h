#ifndef SIMPLE_COMPUTE_H
#define SIMPLE_COMPUTE_H

// #define VK_NO_PROTOTYPES
#include "../../render/compute_common.h"

#include <etna/GlobalContext.hpp>
#include <etna/ComputePipeline.hpp>

#include <string>
#include <iostream>
#include <memory>

class SimpleCompute : public ICompute
{
public:
  SimpleCompute(uint32_t a_length);
  ~SimpleCompute()  { Cleanup(); };

  inline vk::Instance GetVkInstance() const override { return m_context->getInstance(); }
  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void Execute() override;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
  etna::GlobalContext *m_context;
  vk::CommandBuffer m_cmdBufferCompute = VK_NULL_HANDLE;
  vk::Fence m_fence;

  uint32_t m_length  = 16u;
  
  vk::PhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char *> m_deviceExtensions       = {};
  std::vector<const char *> m_instanceExtensions     = {};
  
  etna::ComputePipeline m_pipeline;

  etna::Buffer m_A, m_B, m_sum;
 
  void BuildCommandBufferSimple(vk::CommandBuffer a_cmdBuff, vk::Pipeline a_pipeline);

  void SetupSimplePipeline();
  void loadShaders();
  void CleanupPipeline();

  void Cleanup();
};


#endif //SIMPLE_COMPUTE_H
