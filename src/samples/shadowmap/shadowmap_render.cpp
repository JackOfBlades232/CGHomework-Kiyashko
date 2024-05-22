#include "shadowmap_render.h"
#include "etna/Image.hpp"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"

#include <cstdint>
#include <cstring>
#include <geom/vk_mesh.h>
#include <vector>
#include <vk_pipeline.h>
#include <vk_buffers.h>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewColor = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_color",
    .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc
  });
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  m_uboMappedMem = constants.map();

  particles = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = 256 * sizeof(Particle),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY, // sorry
    .name = "particles_data",
  });
  memset((Particle*)particles.map(), 0, 256 * sizeof(Particle));

  particleMatrices = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = 256 * sizeof(LiteMath::float4x4),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "particles_matrices",
  });

  particleIndices = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = (256 + 1) * sizeof(uint32_t),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "particles_indices",
  });

  particleDrawIndirectCmd = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = 4*sizeof(uint32_t) + sizeof(int32_t),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "particles_cmd",
  });
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  loadShaders();
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
  particleDrawIndirectCmd.reset();
  particleIndices.reset();
  particleMatrices.reset();
  particles.reset();
  mainViewColor.reset();
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  


  constants = etna::Buffer();
}


/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pQuad = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{ 
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .rect = { 0, 0, 512, 512 }, 
    });
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("sss_blur", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/subsurface_scatter.comp.spv"});
  
  etna::create_program("particles_emit", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/particles_emit.comp.spv"});
  etna::create_program("particles_simulate", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/particles_simulate.comp.spv"});
  etna::create_program("particles_sort", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/particles_sort.comp.spv"});
  etna::create_program("part_forward",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/particles.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/particles.vert.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });

  m_sssPipeline = pipelineManager.createComputePipeline("sss_blur", {});

  m_partEmitPipeline = pipelineManager.createComputePipeline("particles_emit", {});
  m_partSimulatePipeline = pipelineManager.createComputePipeline("particles_simulate", {});
  m_partSortPipeline = pipelineManager.createComputePipeline("particles_sort", {});

  m_partForwardPipeline = pipelineManager.createGraphicsPipeline("part_forward",
    {
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable         = true,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp        = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp        = vk::BlendOp::eAdd,
            .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
          }
        },
        .logicOpEnable = false
      },
      .depthConfig = {
        .depthTestEnable  = true,
        .depthWriteEnable = false,
        .depthCompareOp   = vk::CompareOp::eLess
      },
      .fragmentShaderOutput = {
        .colorAttachmentFormats = { static_cast<vk::Format>(m_swapchain.GetFormat()) },
        .depthAttachmentFormat  = vk::Format::eD32Sfloat
      },
    });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    auto inst         = m_pScnMgr->GetInstanceInfo(i);
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
    vkCmdPushConstants(a_cmdBuff, a_pipelineLayout,
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  if (drawParticles)
    RecordParticlesComputePass(a_cmdBuff);

  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048}, {}, {.image = shadowMap.get(), .view = shadowMap.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    DrawSceneCmd(a_cmdBuff, m_lightMatrix, m_shadowPipeline.getVkPipelineLayout());
  }

  //// draw final scene to screen
  //
  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_material");

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height},
      {{.image = mainViewColor.get(), .view = mainViewColor.getView({})}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj, m_basicForwardPipeline.getVkPipelineLayout());
  }

  if (useSSS)
    RecordSSSCommands(a_cmdBuff);

  if (drawParticles)
    RecordParticlesForwardPass(a_cmdBuff, m_worldViewProj);

  RecordBlitMainColorToRTCommands(a_cmdBuff, a_targetImage);

  if(m_input.drawFSQuad)
    m_pQuad->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, shadowMap, defaultSampler);

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleShadowmapRender::RecordSSSCommands(VkCommandBuffer a_cmdBuff)
{
  etna::set_state(a_cmdBuff, mainViewColor.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite),
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  etna::set_state(a_cmdBuff, mainViewDepth.get(), 
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlags2(vk::AccessFlagBits2::eShaderSampledRead),
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eDepth);
  etna::flush_barriers(a_cmdBuff);

  auto sssInfo = etna::get_shader_program("sss_blur");
  auto set = etna::create_descriptor_set(sssInfo.getDescriptorLayoutId(0), a_cmdBuff,
  {
    etna::Binding {0, mainViewColor.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
    etna::Binding {1, mainViewDepth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
  });
  VkDescriptorSet vkSet = set.getVkSet();

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_sssPipeline.getVkPipeline());
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
    m_sssPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

  uint32_t wgDimX = (m_width-1)/16 + 1;
  uint32_t wgDimY = (m_height-1)/16 + 1;

  sssParams.isHorizontal = 1.f;//true;
  vkCmdPushConstants(a_cmdBuff, m_sssPipeline.getVkPipelineLayout(),
    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SSSParams), &sssParams);
  vkCmdDispatch(a_cmdBuff, wgDimX, wgDimY, 1);

  vk::ImageMemoryBarrier2 barrier{
    .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
    .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
    .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
    .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
    .oldLayout           = vk::ImageLayout::eGeneral,
    .newLayout           = vk::ImageLayout::eGeneral,
    .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
    .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
    .image               = mainViewColor.get(),
    .subresourceRange    = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    },
  };
  vk::DependencyInfo depInfo{
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier
  };
  vkCmdPipelineBarrier2(a_cmdBuff, (VkDependencyInfo *)&depInfo);

  sssParams.isHorizontal = 0.f;//false;
  vkCmdPushConstants(a_cmdBuff, m_sssPipeline.getVkPipelineLayout(),
    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SSSParams), &sssParams);
  vkCmdDispatch(a_cmdBuff, wgDimX, wgDimY, 1);
}

void SimpleShadowmapRender::RecordBlitMainColorToRTCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage)
{
  etna::set_state(a_cmdBuff, mainViewColor.get(), 
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlags2(vk::AccessFlagBits2::eTransferRead),
    vk::ImageLayout::eTransferSrcOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::set_state(a_cmdBuff, a_targetImage, 
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlags2(vk::AccessFlagBits2::eTransferWrite),
    vk::ImageLayout::eTransferDstOptimal,
    vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(a_cmdBuff);

  VkImageBlit blit;
  blit.srcSubresource.aspectMask     = (VkImageAspectFlags)vk::ImageAspectFlagBits::eColor;
  blit.srcSubresource.mipLevel       = 0;
  blit.srcSubresource.baseArrayLayer = 0;
  blit.srcSubresource.layerCount     = 1;
  blit.srcOffsets[0]                 = { 0, 0, 0 };
  blit.srcOffsets[1]                 = { (int32_t)m_width, (int32_t)m_height, 1 };
  blit.dstSubresource.aspectMask     = (VkImageAspectFlags)vk::ImageAspectFlagBits::eColor;
  blit.dstSubresource.mipLevel       = 0;
  blit.dstSubresource.baseArrayLayer = 0;
  blit.dstSubresource.layerCount     = 1;
  blit.dstOffsets[0]                 = { 0, 0, 0 };
  blit.dstOffsets[1]                 = { (int32_t)m_width, (int32_t)m_width, 1 };

  vkCmdBlitImage(
    a_cmdBuff,
    mainViewColor.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    a_targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &blit,
    VK_FILTER_LINEAR);
}

void SimpleShadowmapRender::RecordParticlesComputePass(VkCommandBuffer a_cmdBuff)
{
  {
    auto emitInfo = etna::get_shader_program("particles_emit");
    auto set = etna::create_descriptor_set(emitInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, particles.genBinding()},
      etna::Binding {1, particleIndices.genBinding()},
    });
    VkDescriptorSet vkSet = set.getVkSet();

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_partEmitPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_partEmitPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdPushConstants(a_cmdBuff, m_partEmitPipeline.getVkPipelineLayout(),
      VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlesEmmisionParams), &emissionParams);
    vkCmdDispatch(a_cmdBuff, 256/16, 1, 1);
  }
  
  {
    vk::BufferMemoryBarrier2 barriers[] = {
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particles.get(),
        .offset              = 0,
        .size                = 256 * sizeof(Particle)
      },
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particleIndices.get(),
        .offset              = 0,
        .size                = (256 + 1) * sizeof(uint32_t)
      }
    };
    vk::DependencyInfo depInfo{
      .bufferMemoryBarrierCount = 2,
      .pBufferMemoryBarriers    = barriers
    };
    vkCmdPipelineBarrier2(a_cmdBuff, (VkDependencyInfo *)&depInfo);
  }

  {
    auto simInfo = etna::get_shader_program("particles_simulate");
    auto set = etna::create_descriptor_set(simInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, particles.genBinding()},
      etna::Binding {1, particleMatrices.genBinding()},
      etna::Binding {2, particleIndices.genBinding()},
    });
    VkDescriptorSet vkSet = set.getVkSet();

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_partSimulatePipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_partSimulatePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdPushConstants(a_cmdBuff, m_partSimulatePipeline.getVkPipelineLayout(),
      VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlesSimulationParams), &simulationParams);
    vkCmdDispatch(a_cmdBuff, 256/16, 1, 1);
  }
  
  {
    vk::BufferMemoryBarrier2 barriers[] = {
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particles.get(),
        .offset              = 0,
        .size                = 256 * sizeof(Particle)
      },
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eVertexShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particleMatrices.get(),
        .offset              = 0,
        .size                = 256 * sizeof(float4x4)
      },
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particleIndices.get(),
        .offset              = 0,
        .size                = (256 + 1) * sizeof(uint32_t)
      }
    };
    vk::DependencyInfo depInfo{
      .bufferMemoryBarrierCount = 2,
      .pBufferMemoryBarriers    = barriers
    };
    vkCmdPipelineBarrier2(a_cmdBuff, (VkDependencyInfo *)&depInfo);
  }

  {
    auto sortInfo = etna::get_shader_program("particles_sort");
    auto set = etna::create_descriptor_set(sortInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, particles.genBinding()},
      etna::Binding {1, particleIndices.genBinding()},
      etna::Binding {2, particleDrawIndirectCmd.genBinding()},
    });
    VkDescriptorSet vkSet = set.getVkSet();

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_partSortPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_partSortPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDispatch(a_cmdBuff, 1, 1, 1);
  }

  {
    vk::BufferMemoryBarrier2 barriers[] = {
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eVertexShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderStorageRead,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particleIndices.get(),
        .offset              = 0,
        .size                = (256 + 1) * sizeof(uint32_t)
      },
      {
        .srcStageMask        = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask       = vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eDrawIndirect,
        .dstAccessMask       = vk::AccessFlagBits2::eIndirectCommandRead,
        .srcQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .dstQueueFamilyIndex = m_context->getQueueFamilyIdx(),
        .buffer              = particleDrawIndirectCmd.get(),
        .offset              = 0,
        .size                = 4 * sizeof(uint32_t)
      },
    };
    vk::DependencyInfo depInfo{
      .bufferMemoryBarrierCount = 2,
      .pBufferMemoryBarriers    = barriers
    };
    vkCmdPipelineBarrier2(a_cmdBuff, (VkDependencyInfo *)&depInfo);
  }
}

void SimpleShadowmapRender::RecordParticlesForwardPass(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  auto forawrdInfo = etna::get_shader_program("part_forward");
  auto set = etna::create_descriptor_set(forawrdInfo.getDescriptorLayoutId(0), a_cmdBuff,
  {
    etna::Binding {0, particleMatrices.genBinding()},
    etna::Binding {1, particleIndices.genBinding()},
    etna::Binding {2, particles.genBinding()},
  });
  VkDescriptorSet vkSet = set.getVkSet();

  etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height},
      {{.image = mainViewColor.get(), .view = mainViewColor.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_partForwardPipeline.getVkPipeline());
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_partForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

  vkCmdPushConstants(a_cmdBuff, m_partForwardPipeline.getVkPipelineLayout(),
    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4x4), &a_wvp);
  vkCmdDrawIndirect(a_cmdBuff, particleDrawIndirectCmd.get(), 0, 1, 4 * sizeof(uint32_t));
}
