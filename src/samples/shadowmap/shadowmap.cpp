#include "shadowmap_render.h"


/// RESOURCE ALLOCATION

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
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });
  vsmSmoothMomentMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = shadowmapExtent,
    .name = "vsm_smooth_moment_map",
    .format = vk::Format::eR32G32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled
  });
}

void SimpleShadowmapRender::DeallocateShadowmapResources()
{
  shadowMap.reset();
  vsmMomentMap.reset();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::LoadShadowmapShaders()
{
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});

  etna::create_program("vsm_shadow",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_shadowmap.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
  etna::create_program("vsm_filtering", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_filter.comp.spv"});
}

void SimpleShadowmapRender::SetupShadowmapPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
      .bindings = {etna::VertexShaderInputDescription::Binding{
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_simpleShadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_terrainSimpleShadowPipeline = pipelineManager.createGraphicsPipeline("terrain_simple_shadow",
    {
      .inputAssemblyConfig  = { .topology = vk::PrimitiveTopology::ePatchList },
      .tessellationConfig   = { .patchControlPoints = 4 },
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });

  m_vsmShadowPipeline = pipelineManager.createGraphicsPipeline("vsm_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32Sfloat},
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_terrainVsmPipeline = pipelineManager.createGraphicsPipeline("terrain_vsm_shadow",
    {
      .inputAssemblyConfig  = { .topology = vk::PrimitiveTopology::ePatchList },
      .tessellationConfig   = { .patchControlPoints = 4 },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32Sfloat},
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_vsmFilteringPipeline = pipelineManager.createComputePipeline("vsm_filtering", {});
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordShadowPassCommands(VkCommandBuffer a_cmdBuff)
{
  if (currentShadowmapTechnique == eShTechNone) 
    return;

  etna::GraphicsPipeline *shadowmapPipeline = nullptr;
  std::vector<etna::RenderTargetState::AttachmentParams> colorAttachments{};
  switch (currentShadowmapTechnique)
  {
  case eSimple:
  case ePcf:
    shadowmapPipeline = &m_simpleShadowPipeline;
    break;
  case eVsm:
    shadowmapPipeline = &m_vsmShadowPipeline;
    colorAttachments  = {{.image = vsmMomentMap.get(), .view = vsmMomentMap.getView({})}};
    break;
  }

  etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048},
    colorAttachments, {.image = shadowMap.get(), .view = shadowMap.getView({})});

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmapPipeline->getVkPipeline());
  RecordDrawSceneCommands(a_cmdBuff, m_lightMatrix, shadowmapPipeline->getVkPipelineLayout());

  RecordDrawTerrainToShadowmapCommands(a_cmdBuff, m_lightMatrix);
}

void SimpleShadowmapRender::RecordShadowmapProcessingCommands(VkCommandBuffer a_cmdBuff)
{
  if (currentShadowmapTechnique == eVsm)
  {
    // Filter the shadowmap
    etna::set_state(a_cmdBuff, vsmMomentMap.get(), 
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead),
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);
    etna::set_state(a_cmdBuff, vsmSmoothMomentMap.get(), 
      vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlags2(vk::AccessFlagBits2::eShaderWrite),
      vk::ImageLayout::eGeneral,
      vk::ImageAspectFlagBits::eColor);
    etna::flush_barriers(a_cmdBuff);

    auto programInfo = etna::get_shader_program("vsm_filtering");
    auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(0), a_cmdBuff, { 
      etna::Binding{0, vsmMomentMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{1, vsmSmoothMomentMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    uint32_t wgDim = (2048 - 1) / VSM_WORK_GROUP_DIM + 1;

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_vsmFilteringPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_vsmFilteringPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    vkCmdDispatch(a_cmdBuff, wgDim, wgDim, 1);

    etna::set_state(a_cmdBuff, vsmSmoothMomentMap.get(), vk::PipelineStageFlagBits2::eAllGraphics,
      vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead), vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);
    etna::flush_barriers(a_cmdBuff);
  }
}
