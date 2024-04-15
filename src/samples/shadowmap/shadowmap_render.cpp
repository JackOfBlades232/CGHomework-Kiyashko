#include "shadowmap_render.h"
#include "../../../resources/shaders/common.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainRt = RenderTarget(etna::Image::CreateInfo
    {
       .extent     = vk::Extent3D{m_width, m_height, 1},
       .name       = "main_view_rt",
       .format     = static_cast<vk::Format>(m_swapchain.GetFormat()),
       .imageUsage = vk::ImageUsageFlagBits::eColorAttachment
                     | vk::ImageUsageFlagBits::eSampled
                     | vk::ImageUsageFlagBits::eTransferSrc
                     | vk::ImageUsageFlagBits::eTransferDst
    },
    m_context);

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
  AllocateAAResources();
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
  DeallocateAAResources();
  DeallocateShadowmapResources();
  DeallocateDeferredResources();

  mainRt.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::LoadShaders()
{
  LoadDeferredShaders();
  LoadShadowmapShaders();
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
  SetupAAPipelines();
  SetupTerrainPipelines();
  SetupVolfogPipelines();
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
  RecordShadowmapProcessingCommands(a_cmdBuff);

  // Deferred
  RecordGeomPassCommands(a_cmdBuff);
  RecordResolvePassCommands(a_cmdBuff);

  // Anti-aliasing
  RecordAAResolveCommands(a_cmdBuff);

  // @TODO(PKiyashko): move depth-based screen space effects to before aa

  if (volfogEnabled)
    RecordVolfogCommands(a_cmdBuff, m_worldViewProj);

  BlitMainRTToScreen(a_cmdBuff, a_targetImage, a_targetImageView);

  if (m_input.drawFSQuad)
    m_pQuad->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, volfogMap, defaultSampler);

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

