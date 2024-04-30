#include "shadowmap_render.h"

/// GBUFFER

SimpleShadowmapRender::GBuffer::GBuffer(vk::Extent2D extent, etna::GlobalContext *ctx, const char *name)
{
  std::string nname   = (name ? std::string(name) : "") + std::string("_gbuf_norm");
  std::string dname   = (name ? std::string(name) : "") + std::string("_gbuf_depth");
  std::string aoname  = (name ? std::string(name) : "") + std::string("_gbuf_ao");
  std::string baoname = (name ? std::string(name) : "") + std::string("_gbuf_blurred_ao");

  normals = ctx->createImage(etna::Image::CreateInfo
    {
      .extent     = vk::Extent3D{extent.width, extent.height, 1},
      .name       = nname,
      .format     = vk::Format::eR32G32B32A32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
    });
  depth = ctx->createImage(etna::Image::CreateInfo
    {
      .extent     = vk::Extent3D{extent.width, extent.height, 1},
      .name       = dname,
      .format     = vk::Format::eD32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
    });
  ao = ctx->createImage(etna::Image::CreateInfo
    {
      .extent     = vk::Extent3D{extent.width, extent.height, 1},
      .name       = aoname,
      .format     = vk::Format::eR32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
    });
  blurredAo = ctx->createImage(etna::Image::CreateInfo
    {
      .extent     = vk::Extent3D{extent.width, extent.height, 1},
      .name       = baoname,
      .format     = vk::Format::eR32Sfloat,
      .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled
    });
}


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateDeferredResources()
{
  mainGbuffer = GBuffer(vk::Extent2D{m_width, m_height}, m_context, "main");
}

void SimpleShadowmapRender::DeallocateDeferredResources()
{
  mainGbuffer.reset();
}


/// PIPELINES CREATION

// @TODO(PKiyashko): pull out this generic pipeline creation to some utils thing.
void SimpleShadowmapRender::LoadDeferredShaders()
{
  etna::create_program("simple_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("shadow_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("vsm_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/vsm_shadow.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"
    });
  etna::create_program("pcf_resolve",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/pcf_shadow.frag.spv", 
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
  m_geometryPipeline    = pipelineManager.createGraphicsPipeline("deferred_gpass",
    {
      .vertexShaderInput    = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });

  m_terrainGpassPipeline = pipelineManager.createGraphicsPipeline("terrain_gpass",
    {
      .inputAssemblyConfig  = { .topology = vk::PrimitiveTopology::ePatchList },
      .tessellationConfig   = { .patchControlPoints = 4 },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
}

void SimpleShadowmapRender::RebuildCurrentDeferredPipelines()
{
  m_pGbufferResolver = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName   = CurrentResolveProgramName(),
      .programExists = true,
      .format        = vk::Format::eR16G16B16A16Sfloat,
      .extent        = CurrentRTRect().extent
    });

  m_pSsao = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName   = "generate_ssao",
      .programExists = true,
      .format        = vk::Format::eR32Sfloat,
      .extent        = CurrentRTRect().extent
    });

  m_pLowresRsmResolver = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName = "rsm_lowres_resolve",
      .programExists = true,
      .format = vk::Format::eR32G32B32A32Sfloat,
      .extent = vk::Extent2D{GetLowresRsmRect().extent.width, GetLowresRsmRect().extent.height}
    });
}


/// TECHNIQUE CHOICE

const char *SimpleShadowmapRender::CurrentResolveProgramName()
{
  switch (currentShadowmapTechnique)
  {
  case eShadowmapNone:
    return "simple_resolve";
    break;
  case eSimple:
    return "shadow_resolve";
    break;
  case ePcf:
    return "pcf_resolve";
    break;
  case eVsm:
    return "vsm_resolve";
    break;
  }
}

SimpleShadowmapRender::GBuffer &SimpleShadowmapRender::CurrentGbuffer()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return ssaaGbuffer;
  default:
    return mainGbuffer;
  }
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordGeomPassCommands(VkCommandBuffer a_cmdBuff)
{
  GBuffer &gbuf = CurrentGbuffer();
  etna::RenderTargetState renderTargets(a_cmdBuff, CurrentRTRect(),
    {{.image = gbuf.normals.get(), .view = gbuf.normals.getView({})}},
    {.image = gbuf.depth.get(), .view = gbuf.depth.getView({})});

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometryPipeline.getVkPipeline());
  RecordDrawSceneCommands(a_cmdBuff, m_worldViewProj, m_geometryPipeline.getVkPipelineLayout());

  RecordDrawTerrainGpassCommands(a_cmdBuff, m_worldViewProj);
}

void SimpleShadowmapRender::RecordResolvePassCommands(VkCommandBuffer a_cmdBuff)
{
  GBuffer &gbuf = CurrentGbuffer();
  auto attachments = CurrentRTAttachments();
  auto bindings = CurrentRTBindings();

  // Gbuffer to dSet 1
  bindings.push_back(
    { 
      etna::Binding{0, gbuf.depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{1, gbuf.normals.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}, 
      etna::Binding{2, gbuf.blurredAo.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{3, rsmLowresFrame.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

  // @TODO(PKiyashko): add ability to pass multiple attachments to postfx rederer? This basically
  //                   assumes that there is always one attachment
  m_pGbufferResolver->RecordCommands(a_cmdBuff, attachments[0].image, attachments[0].view, std::move(bindings));
}

