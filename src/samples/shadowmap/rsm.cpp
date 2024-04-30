#include "etna/DescriptorSet.hpp"
#include "etna/Etna.hpp"
#include "shadowmap_render.h"

#include <cmath>
#include <random>
#include <vulkan/vulkan_core.h>

void SimpleShadowmapRender::AllocateRsmResources()
{
  const vk::Extent3D rsmExtent{GetLowresRsmRect().extent.width, GetLowresRsmRect().extent.height, 1};
  rsmPositions = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = rsmExtent,
    .name = "rsm_positions",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });
  rsmNormals = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = rsmExtent,
    .name = "rsm_normals",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });
  rsmDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = rsmExtent,
    .name = "rsm_depth",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });
  rsmFlux = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = rsmExtent,
    .name = "rsm_flux",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  rsmLowresFrame = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = rsmExtent,
    .name = "rsm_lowres_frame",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  static_assert(sizeof(LiteMath::float4) * RSM_DISTIBUTION_SIZE == sizeof(rsmSampleParams));
  rsmSamplesDistribution = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(rsmSampleParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "rsm_samples_dist"
  });

  // @TODO(PKiyashko): pull out constants to either params, or somewhere visible
  const float rMax = 0.05f;

  std::uniform_real_distribution<float> random(0.f, 1.f);
  std::default_random_engine engine;
  float weightSum = 0.f;
  for (size_t i = 0; i < RSM_DISTIBUTION_SIZE; ++i)
  {
    float angle    = 2.f * F_PI * random(engine);
    float radCoeff = random(engine);
    float rad      = rMax * radCoeff;
    float weight   = radCoeff;

    rsmSampleParams[i].x = rad * sinf(angle);
    rsmSampleParams[i].y = rad * cosf(angle);
    rsmSampleParams[i].z = weight;
    rsmSampleParams[i].w = 0.f;// unused

    weightSum += weight;
  }
  for (size_t i = 0; i < RSM_DISTIBUTION_SIZE; ++i)
    rsmSampleParams[i].z /= weightSum;

  memcpy(rsmSamplesDistribution.map(), rsmSampleParams, sizeof(rsmSampleParams));
}

void SimpleShadowmapRender::DeallocateRsmResources()
{
  rsmPositions.reset();
  rsmNormals.reset();
  rsmDepth.reset();
  rsmFlux.reset();
  rsmLowresFrame.reset();

  rsmSamplesDistribution.reset();
}

void SimpleShadowmapRender::LoadRsmShaders()
{
  etna::create_program("rsm_shadow",
    {
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/rsm_shadowmap.frag.spv", 
      VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
    });
}

void SimpleShadowmapRender::SetupRsmPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
      .bindings = {etna::VertexShaderInputDescription::Binding{
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };


  vk::PipelineColorBlendAttachmentState blendState{
    .blendEnable = vk::False,
    .colorWriteMask = vk::ColorComponentFlagBits::eR 
                      | vk::ColorComponentFlagBits::eG 
                      | vk::ColorComponentFlagBits::eB 
                      | vk::ColorComponentFlagBits::eA,
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_rsmShadowPipeline = pipelineManager.createGraphicsPipeline("rsm_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .blendingConfig = {.attachments = {blendState, blendState, blendState}},
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat},
          .depthAttachmentFormat = vk::Format::eD16Unorm
        },
    });

  m_pLowresRsmResolver = std::make_unique<PostfxRenderer>(PostfxRenderer::CreateInfo{
      .programName = "rsm_lowres_resolve",
      .fragShaderPath = VK_GRAPHICS_BASIC_ROOT"/resources/shaders/rsm_resolve.frag.spv",
      .format = vk::Format::eR32G32B32A32Sfloat,
      .extent = vk::Extent2D{GetLowresRsmRect().extent.width, GetLowresRsmRect().extent.height}
    });
}

void SimpleShadowmapRender::RecordRsmShadowPassCommands(VkCommandBuffer a_cmdBuff)
{
  auto programInfo = etna::get_shader_program("rsm_shadow");
  auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(0), a_cmdBuff, 
    { etna::Binding{ 0, constants.genBinding() } });
  VkDescriptorSet vkSet = set.getVkSet();

  etna::RenderTargetState renderTargets(a_cmdBuff, GetLowresRsmRect(),
    {
      {.image = rsmPositions.get(), .view = rsmPositions.getView({})},
      {.image = rsmNormals.get(), .view = rsmNormals.getView({})},
      {.image = rsmFlux.get(), .view = rsmFlux.getView({})}
    },
    {.image = rsmDepth.get(), .view = rsmDepth.getView({})});

  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rsmShadowPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

  vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rsmShadowPipeline.getVkPipeline());
  RecordDrawSceneCommands(a_cmdBuff, m_lightMatrix, m_rsmShadowPipeline.getVkPipelineLayout());
}

void SimpleShadowmapRender::RecordRsmIndirectLightingCommands(VkCommandBuffer a_cmdBuff)
{
  m_pLowresRsmResolver->RecordCommands(a_cmdBuff, rsmLowresFrame.get(), rsmLowresFrame.getView({}), 
    {{ 
      etna::Binding{0, constants.genBinding()},
      etna::Binding{1, CurrentGbuffer().depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{2, rsmPositions.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{3, rsmNormals.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{4, rsmFlux.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{5, rsmSamplesDistribution.genBinding()},
    }});
}

