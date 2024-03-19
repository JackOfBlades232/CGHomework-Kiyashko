#include "quad2d_render.h"

#include <etna/Etna.hpp>
#include <etna/Vulkan.hpp>
#include <vk_utils.h>


/// RESOURCE ALLOCATION

void Quad2D_Render::AllocateResources()
{
  m_defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
}

static std::vector<uint32_t> load_bmp(const char *filename, uint32_t *pW, uint32_t *pH)
{
  FILE *f = fopen(filename, "rb");

  if (f == nullptr)
  {
    (*pW) = 0;
    (*pH) = 0;
    std::cout << "can't open file" << std::endl;
    return {};
  }

  std::byte info[54];
  auto readRes = fread(info, sizeof(std::byte), 54, f);// read the 54-byte header
  if (readRes != 54)
  {
    std::cout << "can't read 54 byte BMP header" << std::endl;
    return {};
  }

  uint32_t width  = *(uint32_t *)&info[18];
  uint32_t height = *(uint32_t *)&info[22];

  uint32_t row_padded = (width * 3 + 3) & (~3);
  auto data      = new std::byte[row_padded];

  std::vector<uint32_t> res(width * height);

  for (uint32_t i = 0; i < height; i++)
  {
    fread(data, sizeof(std::byte), row_padded, f);
    for (uint32_t j = 0; j < width; j++)
      res[i * width + j] = (uint32_t(data[j * 3 + 0]) << 16) | (uint32_t(data[j * 3 + 1]) << 8) | (uint32_t(data[j * 3 + 2]) << 0);
  }

  fclose(f);
  delete[] data;

  (*pW) = width;
  (*pH) = height;
  return res;
}

void Quad2D_Render::LoadScene(const char *, bool)
{
  m_imageData.content = load_bmp("../resources/textures/texture1.bmp", &m_imageData.w, &m_imageData.h);

  // @NOTE(PKiyashko): this is done here because AllocateResources may be called before LoadScene
  // @TODO(PKiyashko): remake this with dedicated etna copy helper
  vk::CommandBuffer cmdBuff = vk_utils::createCommandBuffer(m_context->getDevice(), m_commandPool);
  m_fullscreenImage = etna::create_image_from_bytes(etna::Image::CreateInfo
    {
      .extent      = { m_imageData.w, m_imageData.h, 1 },
      .name        = "target_image",
      .format      = vk::Format::eR8G8B8A8Unorm,
      .imageUsage  = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY
    }, 
    cmdBuff, m_imageData.content.data());
  m_context->getDevice().freeCommandBuffers(m_commandPool, {cmdBuff});

  // TODO: Make a separate stage
  SetupQuadRenderer();
}

void Quad2D_Render::DeallocateResources()
{
  m_fullscreenImage.reset();
  m_swapchain.Cleanup();
  m_context->getInstance().destroySurfaceKHR(m_surface);
}


/// PIPELINE PREPARATION

void Quad2D_Render::SetupQuadRenderer()
{
  // create full screen quad for debug purposes
  // 
  m_pQuad = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{ 
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .rect = { 0, 0, m_width, m_height }, 
    });
}


/// COMMAND BUFFER FILLING

void Quad2D_Render::BuildCommandBufferSimple(vk::CommandBuffer a_cmdBuff, vk::Image a_targetImage, vk::ImageView a_targetImageView)
{
  a_cmdBuff.reset();

  ETNA_CHECK_VK_RESULT(a_cmdBuff.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse }));

  m_pQuad->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, m_fullscreenImage, m_defaultSampler);

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  ETNA_CHECK_VK_RESULT(a_cmdBuff.end());
}
