#include "postfx_renderer.h"

#include "etna/GlobalContext.hpp"
#include "etna/Etna.hpp"
#include "etna/RenderTargetStates.hpp"


PostfxRenderer::PostfxRenderer(CreateInfo info) 
{
  m_extent = info.extent;
  m_programId = etna::create_program(info.programName, {
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    info.fragShaderPath
  });

  auto &pipelineManager = etna::get_context().getPipelineManager();
  m_pipeline = pipelineManager.createGraphicsPipeline(info.programName,
    {
      .multisampleConfig = { .rasterizationSamples = info.sampleCountFlags },
      .fragmentShaderOutput = { .colorAttachmentFormats = {info.format} }
    });
}

void PostfxRenderer::RecordCommands(
  vk::CommandBuffer cmdBuff,
  vk::Image targetImage,
  vk::ImageView targetImageView,
  std::vector<std::vector<etna::Binding>> &&bindingSets)
{
  auto programInfo = etna::get_shader_program(m_programId);

  std::vector<etna::DescriptorSet> sets(bindingSets.size());
  std::vector<vk::DescriptorSet> vkSets(bindingSets.size());
  for (size_t i = 0; i < bindingSets.size(); ++i)
  {
    if (bindingSets[i].size() == 0)
      continue;
    auto set = etna::create_descriptor_set(programInfo.getDescriptorLayoutId(i), cmdBuff, std::move(bindingSets[i]));
    vkSets[i] = set.getVkSet();
    sets[i] = std::move(set);
  }

  etna::RenderTargetState renderTargets(cmdBuff, 
    vk::Rect2D{0, 0, m_extent.width, m_extent.height}, 
    {{.image = targetImage, .view = targetImageView}}, {});

  cmdBuff.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getVkPipeline());
  cmdBuff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline.getVkPipelineLayout(), 0, vkSets, {});

  cmdBuff.draw(3, 1, 0, 0);
}
