#include "shadowmap_render.h"
#include <cmath>
#include <random>

/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateSSAOResources()
{
  ssaoConstants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(SSAOUniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "ssao_constants"
  });

  // @HUH(PKiyashko): maybe move to a more verbosely separate stage?
  std::uniform_real_distribution<float> random(0.f, 1.f);
  std::default_random_engine engine;
  for (size_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
  {
    // Random sample in hemisphere
    float alpha = random(engine) * F_PI;
    float beta  = random(engine) * F_PI;
    float r     = random(engine);

    float3 unitV = float3(cosf(alpha)*cosf(beta), sinf(alpha)*cosf(beta), sinf(beta)) * r;
    ssaoUniforms.kernel[i] = float4(unitV.x, unitV.y, unitV.z, 1.f);
  }
  for (size_t i = 0; i < SSAO_TANGENTS_DIM*SSAO_TANGENTS_DIM; ++i)
    ssaoUniforms.tangentBases[i] = float4(random(engine)*2.f - 1.f, random(engine)*2.f - 1.f, 0.f, 1.f);

  memcpy(ssaoConstants.map(), &ssaoUniforms, sizeof(ssaoUniforms));
}

void SimpleShadowmapRender::DeallocateSSAOResources()
{
  ssaoConstants.reset();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::LoadSSAOShaders()
{  
  etna::create_program("generate_ssao",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/generate_ssao.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("blur_ssao", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/blur_ssao.comp.spv"});
}

void SimpleShadowmapRender::SetupSSAOPipelines()
{
  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_blurSsaoPipeline    = pipelineManager.createComputePipeline("blur_ssao", {});
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordSSAOGenerationCommands(VkCommandBuffer a_cmdBuff)
{
  GBuffer &gbuf = CurrentGbuffer();
  m_pSsao->RecordCommands(a_cmdBuff, gbuf.ao.get(), gbuf.ao.getView({}), 
    {{ 
      etna::Binding{0, constants.genBinding()},
      etna::Binding{1, ssaoConstants.genBinding()},
      etna::Binding{2, gbuf.depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{3, gbuf.normals.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}, 
    }});
}

void SimpleShadowmapRender::RecordSSAOBlurCommands(VkCommandBuffer a_cmdBuff)
{
  GBuffer &gbuf = CurrentGbuffer();

  etna::set_state(a_cmdBuff, gbuf.ao.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead),
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::set_state(a_cmdBuff, gbuf.blurredAo.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderWrite),
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(a_cmdBuff);

  auto programInfo = etna::get_shader_program("blur_ssao");
  auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(0), a_cmdBuff, { 
    etna::Binding{0, gbuf.ao.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    etna::Binding{1, gbuf.blurredAo.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
    etna::Binding{2, gbuf.depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
  });

  VkDescriptorSet vkSet = set.getVkSet();

  uint32_t wgDimX = (CurrentRTRect().extent.width - 1) / SSAO_WORK_GROUP_DIM + 1;
  uint32_t wgDimY = (CurrentRTRect().extent.height - 1) / SSAO_WORK_GROUP_DIM + 1;

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_blurSsaoPipeline.getVkPipeline());
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
    m_blurSsaoPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
  vkCmdDispatch(a_cmdBuff, wgDimX, wgDimY, 1);

  etna::set_state(a_cmdBuff, gbuf.blurredAo.get(), vk::PipelineStageFlagBits2::eAllGraphics,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead), vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(a_cmdBuff);
}
