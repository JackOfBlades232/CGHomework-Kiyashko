#include "shadowmap_render.h"

/// PIPELINES CREATION

// @TODO(PKiyashko): pull out this generic pipeline creation to some utils thing.
void SimpleShadowmapRender::LoadForwardShaders()
{
  etna::create_program("simple_forward",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
  etna::create_program("shadow_forward",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
  etna::create_program("vsm_forward",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_shadow.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
  etna::create_program("pcf_forward",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/pcf_shadow.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
}

// @TODO(PKiyashko): pull out this generic pipeline creation to some utils thing.
void SimpleShadowmapRender::RebuildCurrentForwardPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = { etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription() } }
  };

  auto &pipelineManager = etna::get_context().getPipelineManager();

  vk::PipelineMultisampleStateCreateInfo multisampleConfig{
    .rasterizationSamples = currentAATechnique == eMsaa ? vk::SampleCountFlagBits::e4 : vk::SampleCountFlagBits::e1
  };

  m_forwardPipeline = pipelineManager.createGraphicsPipeline(CurrentForwardProgramName(),
    { 
      .vertexShaderInput    = sceneVertexInputDesc,
      .multisampleConfig    = multisampleConfig,
      .fragmentShaderOutput = 
      {
        .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
        .depthAttachmentFormat  = vk::Format::eD32Sfloat 
      }
    });

  m_terrainForwardPipeline = pipelineManager.createGraphicsPipeline(CurrentTerrainForwardProgramName(),
    {
      .inputAssemblyConfig  = { .topology = vk::PrimitiveTopology::ePatchList },
      .tessellationConfig   = { .patchControlPoints = 4 },
      .multisampleConfig    = multisampleConfig,
      .fragmentShaderOutput = 
      {
        .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
        .depthAttachmentFormat  = vk::Format::eD32Sfloat 
      }
    });

  m_pVolfogApplier = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName      = "apply_volfog",
      .programExists    = true,
      .format           = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .sampleCountFlags = multisampleConfig.rasterizationSamples,
      .extent           = CurrentRTRect().extent
    });
}


/// TECHNIQUE CHOICE

const char *SimpleShadowmapRender::CurrentForwardProgramName()
{
  switch (currentShadowmapTechnique)
  {
  case eShTechNone:
    return "simple_forward";
    break;
  case eSimple:
    return "shadow_forward";
    break;
  case ePcf:
    return "pcf_forward";
    break;
  case eVsm:
    return "vsm_forward";
    break;
  }
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordForwardPassCommands(VkCommandBuffer a_cmdBuff)
{
  auto programInfo = etna::get_shader_program(CurrentRTProgramName());

  auto bindings = CurrentRTBindings();
  std::vector<etna::DescriptorSet> sets(bindings.size());
  std::vector<VkDescriptorSet> vkSets(bindings.size());
  for (size_t i = 0; i < bindings.size(); ++i)
  {
    if (bindings[i].size() == 0)
      continue;
    auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(i), a_cmdBuff, std::move(bindings[i]));
    vkSets[i] = set.getVkSet();
    sets[i] = std::move(set);
  }

  etna::RenderTargetState renderTargets(a_cmdBuff, 
    CurrentRTRect(),
    CurrentRTAttachments(),
    CurrentRTDepthAttachment());

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_forwardPipeline.getVkPipeline());
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_forwardPipeline.getVkPipelineLayout(), 0, vkSets.size(), vkSets.data(), 0, VK_NULL_HANDLE);

  RecordDrawSceneCommands(a_cmdBuff, m_worldViewProj, m_forwardPipeline.getVkPipelineLayout());

  RecordDrawTerrainForwardCommands(a_cmdBuff, m_worldViewProj);
}

