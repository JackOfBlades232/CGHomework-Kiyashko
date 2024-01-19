#include "simple_compute.h"

#include <etna/Etna.hpp>
#include <etna/Assert.hpp>


void SimpleCompute::Execute()
{
  loadShaders();
  SetupSimplePipeline();

  BuildCommandBufferSimple(m_cmdBufferCompute, nullptr);

  vk::Device device = m_context->getDevice();
  vk::Queue queue = m_context->getQueue();

  // @TODO: check all results
  m_fence = etna::validate_vk_result(device.createFence({}));

  vk::SubmitInfo submitInfo = 
  {
    .commandBufferCount = 1,
    .pCommandBuffers    = &m_cmdBufferCompute
  };
  
  // Submit the command buffer for execution
  ETNA_VK_ASSERT(queue.submit({submitInfo}, m_fence));

  // And wait for the execution to be completed
  ETNA_VK_ASSERT(device.waitForFences({m_fence}, true, UINT64_MAX));

  std::vector<float> values(m_length);
  m_sum.readOnce((std::byte *)values.data(),  sizeof(float) * values.size());

  std::cout << std::endl;
  for (auto v: values) 
  {
    std::cout << v << ' ';
  }
  std::cout << std::endl << std::endl;
}
