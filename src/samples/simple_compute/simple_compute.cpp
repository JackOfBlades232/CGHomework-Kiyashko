#include "simple_compute.h"

#include <etna/Etna.hpp>


void SimpleCompute::loadShaders()
{
  etna::create_program("simple_compute", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.comp.spv"});
}

void SimpleCompute::SetupSimplePipeline()
{
  //// Buffer creation
  //
  m_A = m_context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = sizeof(float) * m_length,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .name = "m_A"
    });

  m_B = m_context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = sizeof(float) * m_length,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .name = "m_B"
    });

  m_sum = m_context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = sizeof(float) * m_length,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
      .name = "m_sum"
    });
  
  //// Filling the buffers
  // 
  std::vector<float> values(m_length);
  for (uint32_t i = 0; i < values.size(); ++i)
    values[i] = (float)i;
  m_A.updateOnce((std::byte *)values.data(), sizeof(float) * values.size());

  for (uint32_t i = 0; i < values.size(); ++i)
    values[i] = (float)i * i;
  m_B.updateOnce((std::byte *)values.data(), sizeof(float) * values.size());

  //// Compute pipeline creation
  auto &pipelineManager = etna::get_context().getPipelineManager();
  m_pipeline = pipelineManager.createComputePipeline("simple_compute", {});
}

void SimpleCompute::CleanupPipeline()
{ 
  if (m_cmdBufferCompute)
    m_context->freeCommandBuffer(m_cmdBufferCompute);

  m_A.reset();
  m_B.reset();
  m_sum.reset();
  m_context->getDevice().destroyFence({m_fence});
}

void SimpleCompute::BuildCommandBufferSimple(vk::CommandBuffer a_cmdBuff, vk::Pipeline a_pipeline)
{
  a_cmdBuff.reset();

  vk::CommandBufferBeginInfo beginInfo = { .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse };

  // Filling the command buffer
  ETNA_VK_ASSERT(a_cmdBuff.begin(beginInfo));

  auto simpleComputeInfo = etna::get_shader_program("simple_compute");

  auto set = etna::create_descriptor_set(simpleComputeInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, m_A.genBinding()},
      etna::Binding {1, m_B.genBinding()},
      etna::Binding {2, m_sum.genBinding()},
    });

  vk::DescriptorSet vkSet = set.getVkSet();

  a_cmdBuff.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline.getVkPipeline());
  a_cmdBuff.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline.getVkPipelineLayout(), 0, {vkSet}, {});

  a_cmdBuff.pushConstants(m_pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(m_length), &m_length);

  etna::flush_barriers(a_cmdBuff);

  a_cmdBuff.dispatch(1, 1, 1);

  ETNA_VK_ASSERT(a_cmdBuff.end());
}
