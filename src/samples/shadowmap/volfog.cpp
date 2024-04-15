#include "shadowmap_render.h"

void SimpleShadowmapRender::AllocateVolfogResources()
{
  auto rtRect = CurrentRTRect();
  volfogMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{rtRect.extent.width/4, rtRect.extent.height/4, 1},
    .name = "volfog_map",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled
  });
}

void SimpleShadowmapRender::DeallocateVolfogResources()
{
  volfogMap.reset();
}

void SimpleShadowmapRender::LoadVolfogShaders()
{
  etna::create_program("generate_volfog", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/generate_volfog.comp.spv"});
  etna::create_program("apply_volfog", 
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/apply_volfog.frag.spv",
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
}

void SimpleShadowmapRender::SetupVolfogPipelines()
{
  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_volfogGeneratePipeline = pipelineManager.createComputePipeline("generate_volfog", {});

  m_pVolfogApplier = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName   = "apply_volfog",
      .programExists = true,
      .format        = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .extent        = vk::Extent2D{m_width, m_height}
    });
}

// @TODO(PKiyashko): this is also a common occurance, pull out to utils
void SimpleShadowmapRender::RecordVolfogCommands(VkCommandBuffer a_cmdBuff, const float4x4 &a_wvp)
{
  //// Generate volfog map
  //
  etna::set_state(a_cmdBuff, volfogMap.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderWrite),
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::set_state(a_cmdBuff, CurrentGbuffer().depth.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead),
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eDepth);
  etna::flush_barriers(a_cmdBuff);

  auto programInfo = etna::get_shader_program("generate_volfog");
  auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(0), a_cmdBuff, 
    { 
      etna::Binding{0, constants.genBinding()},
      etna::Binding{1, volfogMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
      etna::Binding{2, CurrentGbuffer().depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });
  VkDescriptorSet vkSet = set.getVkSet();

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_volfogGeneratePipeline.getVkPipeline());
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
    m_volfogGeneratePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

  float4x4 invViewProj = inverse4x4(a_wvp);
  vkCmdPushConstants(a_cmdBuff, m_volfogGeneratePipeline.getVkPipelineLayout(),
    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(invViewProj), &invViewProj);

  auto rtRect = CurrentRTRect();
  uint32_t wgDimX = (rtRect.extent.width/4 - 1) / VOLFOG_WORK_GROUP_DIM + 1;
  uint32_t wgDimY = (rtRect.extent.height/4 - 1) / VOLFOG_WORK_GROUP_DIM + 1;
  vkCmdDispatch(a_cmdBuff, wgDimX, wgDimY, 1);

  //// Mixin to screen w/ postfx renderer
  //
  mainRt.flip();
  m_pVolfogApplier->RecordCommands(a_cmdBuff, mainRt.current().get(), mainRt.current().getView({}), 
    {{ 
      etna::Binding{0, mainRt.backup().genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{1, volfogMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    }});
}
