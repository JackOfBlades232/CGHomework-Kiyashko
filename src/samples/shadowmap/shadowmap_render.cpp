#include "shadowmap_render.h"
#include "../../../resources/shaders/common.h"
#include "etna/Sampler.hpp"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

/// RENDER TARGET

SimpleShadowmapRender::RenderTarget::RenderTarget(vk::Extent2D extent, vk::Format format, etna::GlobalContext *ctx, const char *name)
  : currentIm(&im[0])
{
  auto fullName = (name ? std::string(name) : "") + std::string("_rt");
  etna::Image::CreateInfo info{
    .extent     = vk::Extent3D{extent.width, extent.height, 1},
    .name       = fullName,
    .format     = format,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment
                  | vk::ImageUsageFlagBits::eSampled
                  | vk::ImageUsageFlagBits::eTransferSrc
                  | vk::ImageUsageFlagBits::eTransferDst
  };
  im[0] = ctx->createImage(info);
  im[1] = ctx->createImage(info);
}


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainRt = RenderTarget(vk::Extent2D{m_width, m_height}, vk::Format::eR16G16B16A16Sfloat, m_context, "main_view");

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  m_uboMappedMem = constants.map();

  AllocateDeferredResources();
  AllocateShadowmapResources();
  AllocateRsmResources();
  AllocateAAResources();
  AllocateSSAOResources();
  AllocateTerrainResources();
  AllocateVolfogResources();
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  LoadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
}

void SimpleShadowmapRender::DeallocateResources()
{
  DeallocateVolfogResources();
  DeallocateTerrainResources();
  DeallocateSSAOResources();
  DeallocateAAResources();
  DeallocateRsmResources();
  DeallocateShadowmapResources();
  DeallocateDeferredResources();

  mainRt.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants.reset();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::LoadShaders()
{
  LoadDeferredShaders();
  LoadShadowmapShaders();
  LoadRsmShaders();
  LoadSSAOShaders();
  LoadTerrainShaders();
  LoadVolfogShaders();
}

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pQuad = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{ 
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .rect = { 0, 0, 512, 512 }, 
    });

  SetupDeferredPipelines();
  SetupShadowmapPipelines();
  SetupRsmPipelines();
  SetupAAPipelines();
  SetupSSAOPipelines();
  SetupTerrainPipelines();
  SetupVolfogPipelines();
  SetupTonemappingPipelines();
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::BuildCommandBuffer(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  // Terrain heightmap generation
  if (needToRegenerateHmap)
  {
    RecordHmapGenerationCommands(a_cmdBuff);
    needToRegenerateHmap = false;
  }

  // Shadowmap
  RecordShadowPassCommands(a_cmdBuff);
  if (useRsm)
    RecordRsmShadowPassCommands(a_cmdBuff);

  RecordShadowmapProcessingCommands(a_cmdBuff);

  // Deferred gpass
  RecordGeomPassCommands(a_cmdBuff);

  // Stuff that relies on the gbuffer
  if (volfogEnabled) 
    RecordVolfogGenerationCommands(a_cmdBuff, m_worldViewProj);
  if (useRsm)
    RecordRsmIndirectLightingCommands(a_cmdBuff);
  if (useSsao)
  {
    RecordSSAOGenerationCommands(a_cmdBuff);
    RecordSSAOBlurCommands(a_cmdBuff);
  }

  RecordResolvePassCommands(a_cmdBuff);

  // Anti-aliasing : resolves aa frame to mainRt
  RecordAAResolveCommands(a_cmdBuff);

  // Postfx stuff that relies on the main rt
  if (volfogEnabled) 
    RecordVolfogApplyCommands(a_cmdBuff);

  // Output to render target through tonemapping from HDR to LDR
  RecordTonemappingCommands(a_cmdBuff, a_targetImage, a_targetImageView);

  if (m_input.drawFSQuad)
    m_pQuad->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, rsmLowresFrame, defaultSampler);

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

