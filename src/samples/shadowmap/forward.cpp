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
void SimpleShadowmapRender::SetupForwardPipelines()
{
};


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordForwardPassCommands(VkCommandBuffer a_cmdBuff)
{

}

