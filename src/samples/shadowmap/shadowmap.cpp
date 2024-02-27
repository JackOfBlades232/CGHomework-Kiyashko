#include "shadowmap_render.h"

void SimpleShadowmapRender::AllocateShadowmapResources()
{
  const vk::Extent3D shadowmapExtent{ 2048, 2048, 1 };

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = shadowmapExtent,
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });
  vsmMomentMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = shadowmapExtent,
    .name = "vsm_moment_map",
    .format = vk::Format::eR32G32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled
  });
}

void SimpleShadowmapRender::DeallocateShadowmapResources()
{
  shadowMap.reset();
  vsmMomentMap.reset();
}

void SimpleShadowmapRender::LoadShadowmapShaders()
{
  etna::create_program("vsm_filtering", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_filter.comp.spv"});
  etna::create_program("vsm_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("pcf_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/pcf_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
}

void SimpleShadowmapRender::SetupShadowmapPipelines(etna::VertexShaderInputDescription sceneVertexInputDesc)
{
  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_simpleShadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_vsmFilteringPipeline = pipelineManager.createComputePipeline("vsm_filtering", {});
  m_vsmForwardPipeline = pipelineManager.createGraphicsPipeline("vsm_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_pcfForwardPipeline = pipelineManager.createGraphicsPipeline("pcf_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
}

etna::GraphicsPipeline &SimpleShadowmapRender::CurrentShadowmapPipeline()
{
  switch (currentShadowmapTechnique)
  {
  case eSimple:
  case eVsm:
  case ePcf:
    return m_simpleShadowPipeline;
  // Not implemented
  case eEsm:
  default:
    return m_simpleShadowPipeline;
  }
}

etna::GraphicsPipeline &SimpleShadowmapRender::CurrentForwardPipeline()
{
  switch (currentShadowmapTechnique)
  {
  case eSimple:
    return m_basicForwardPipeline;
  case eVsm:
    return m_vsmForwardPipeline;
  case ePcf:
    return m_pcfForwardPipeline;
  // Not implemented
  case eEsm:
  default:
    return m_basicForwardPipeline;
  }
}

etna::DescriptorSet SimpleShadowmapRender::CreateCurrentForwardDSet(VkCommandBuffer a_cmdBuff)
{
  switch (currentShadowmapTechnique)
  {
  case eSimple: 
  case ePcf:
  {
    auto materialInfo = etna::get_shader_program("simple_material");
    return std::move(etna::create_descriptor_set(materialInfo.getDescriptorLayoutId(0), a_cmdBuff, {
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) } 
    }));
  }
  case eVsm:
  {
    auto materialInfo = etna::get_shader_program("vsm_material");
    return std::move(etna::create_descriptor_set(materialInfo.getDescriptorLayoutId(0), a_cmdBuff, {
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, vsmMomentMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }
    }));
  }
  // Not implemented
  case eEsm:
  default: 
  {
    auto materialInfo = etna::get_shader_program("simple_material");
    return std::move(etna::create_descriptor_set(materialInfo.getDescriptorLayoutId(0), a_cmdBuff, {
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) } 
    }));
  }
  }
}

void SimpleShadowmapRender::FilterVsmTextureCmd(VkCommandBuffer a_cmdBuff)
{
  etna::set_state(a_cmdBuff, shadowMap.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead),
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eDepth);
  etna::set_state(a_cmdBuff, vsmMomentMap.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderWrite),
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(a_cmdBuff);

  auto programInfo = etna::get_shader_program("vsm_filtering");
  auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(0), a_cmdBuff, { 
    etna::Binding {0, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    etna::Binding {1, vsmMomentMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)}
  });

  VkDescriptorSet vkSet = set.getVkSet();

  uint32_t wgDim = (2048 - 1) / WORK_GROUP_DIM + 1;

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_vsmFilteringPipeline.getVkPipeline());
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
    m_vsmFilteringPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
  vkCmdDispatch(a_cmdBuff, wgDim, wgDim, 1);

  etna::set_state(a_cmdBuff, vsmMomentMap.get(), vk::PipelineStageFlagBits2::eAllGraphics,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead), vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(a_cmdBuff);
}
