#include "shadowmap_render.h"

/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateDeferredResources()
{
  gbuffer.normals = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = vk::Extent3D{m_width, m_height, 1},
    .name       = "gbuf_normals",
    .format     = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  gbuffer.depth = &mainViewDepth;
}

void SimpleShadowmapRender::DeallocateDeferredResources()
{
  gbuffer.normals.reset();
}


/// PIPELINES CREATION

// @TODO(PKiyashko): pull out this generic pipeline creation to some utils thing.
void SimpleShadowmapRender::LoadDeferredShaders()
{
  etna::create_program("simple_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_resolve.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("shadow_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/shadow_resolve.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("vsm_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_resolve.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("pcf_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/pcf_resolve.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });

  etna::create_program("deferred_gpass", 
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_gpass.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
}

void SimpleShadowmapRender::SetupDeferredPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc {
      .bindings = {etna::VertexShaderInputDescription::Binding{
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_forwardPipeline     = pipelineManager.createGraphicsPipeline("deferred_gpass",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
}

void SimpleShadowmapRender::RebuildCurrentDeferredResolvePipeline()
{
  m_pGbufferResolver = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName   = CurrentRTProgramName(),
      .programExists = true,
      .format        = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .extent        = vk::Extent2D{m_width, m_height}
    });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordGeomPassCommands(VkCommandBuffer a_cmdBuff)
{
    etna::RenderTargetState renderTargets(a_cmdBuff, 
      vk::Rect2D{0, 0, m_width, m_height},
      {{.image = gbuffer.normals.get(), .view = gbuffer.normals.getView({})}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometryPipeline.getVkPipeline());
    RecordDrawSceneCmds(a_cmdBuff, m_worldViewProj, m_geometryPipeline.getVkPipelineLayout());
}

void SimpleShadowmapRender::RecordResolvePassCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  auto bindings = CurrentRTBindings();

  // Gbuffer to dSet 1
  bindings.push_back({ 
    etna::Binding{ 0, gbuffer.normals.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }, 
    etna::Binding{ 1, gbuffer.depth->genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) } 
    });

  m_pGbufferResolver->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, std::move(bindings));
}

