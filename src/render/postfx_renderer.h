#ifndef POSTFX_RENDERER_H
#define POSTFX_RENDERER_H

#include <vector>
#include <string>
#include <etna/Vulkan.hpp>
#include "etna/GraphicsPipeline.hpp"
#include "etna/DescriptorSet.hpp"


class PostfxRenderer
{
public:
  struct CreateInfo
  {
    std::string fragShaderPath = nullptr;
    std::string programName = nullptr;
    vk::Format format = vk::Format::eUndefined;
    vk::SampleCountFlagBits sampleCountFlags = vk::SampleCountFlagBits::e1;
    vk::Extent2D extent = {};
  };

  PostfxRenderer(CreateInfo info);

  void RecordCommands(
    vk::CommandBuffer cmdBuff,
    vk::Image targetImage,
    vk::ImageView targetImageView,
    std::vector<std::vector<etna::Binding>> &&bindingSets);

private:
  etna::GraphicsPipeline m_pipeline;
  etna::ShaderProgramId m_programId;
  vk::Extent2D m_extent {};

  PostfxRenderer(const PostfxRenderer &) = delete;
  PostfxRenderer& operator=(const PostfxRenderer &) = delete;
};

#endif
