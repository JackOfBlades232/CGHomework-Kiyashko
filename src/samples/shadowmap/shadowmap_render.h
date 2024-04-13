#ifndef SIMPLE_SHADOWMAP_RENDER_H
#define SIMPLE_SHADOWMAP_RENDER_H

#include "../../render/scene_mgr.h"
#include "../../render/render_common.h"
#include "../../render/quad_renderer.h"
#include "../../render/postfx_renderer.h"
#include "../../../resources/shaders/common.h"
#include "etna/GraphicsPipeline.hpp"
#include <geom/vk_mesh.h>
#include <vk_descriptor_sets.h>
#include <vk_fbuf_attachment.h>
#include <vk_images.h>
#include <vk_swapchain.h>

#include <string>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Sampler.hpp>
#include <etna/RenderTargetStates.hpp>


class IRenderGUI;

class SimpleShadowmapRender : public IRender
{
public:
  struct CreateInfo
  {
    uint32_t width;
    uint32_t height;
  };

  SimpleShadowmapRender(CreateInfo info);
  ~SimpleShadowmapRender();

  uint32_t     GetWidth()      const override { return m_width; }
  uint32_t     GetHeight()     const override { return m_height; }
  VkInstance   GetVkInstance() const override { return m_context->getInstance(); }

  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR &a_surface, bool initGUI) override;

  void ProcessInput(const AppInput& input) override;
  void UpdateCamera(const Camera* cams, uint32_t a_camsNumber) override;
  Camera GetCurrentCamera() override {return m_cam;}
  void UpdateView();

  void LoadScene(const char *path, bool transpose_inst_matrices) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;

private:
  etna::GlobalContext* m_context;
  etna::Image mainViewDepth;
  etna::Sampler defaultSampler;
  etna::Buffer constants;

  // Deferred
  struct GBuffer
  {
    etna::Image normals; 
    etna::Image *depth;
  };
  GBuffer gbuffer;

  // Shadow maps
  etna::Image shadowMap;
  etna::Image vsmMomentMap;
  etna::Image vsmSmoothMomentMap;

  // Anti-aliasing
  // @TODO(PKiyashko): this is a lot of excess resources. Move to recreating instead.
  etna::Image ssaaRt;
  etna::Image ssaaDepth;
  etna::Image msaaRt;
  etna::Image msaaDepth;
  GBuffer ssaaGbuffer;

  etna::Image taaFrames[2];
  etna::Image *taaCurFrame = &taaFrames[0];
  etna::Image *taaPrevFrame = &taaFrames[1];
  etna::Image taaRt;

  VkCommandPool m_commandPool = VK_NULL_HANDLE;

  struct
  {
    uint32_t    currentFrame      = 0u;
    VkQueue     queue             = VK_NULL_HANDLE;
    VkSemaphore imageAvailable    = VK_NULL_HANDLE;
    VkSemaphore renderingFinished = VK_NULL_HANDLE;
  } m_presentationResources;

  std::vector<VkFence> m_frameFences;
  std::vector<VkCommandBuffer> m_cmdBuffersDrawMain;

  struct
  {
    float4x4 projView;
    float4x4 model;
  } pushConst2M;

  float4x4 m_worldViewProj;
  float4x4 m_lightMatrix;    

  // For taa reprojection
  float4x4 m_prevProjViewMatrix;
  float currentReprojectionCoeff = 0.75f;
  bool resetReprojection = true;

  UniformParams m_uniforms {};
  void* m_uboMappedMem = nullptr;

  // Forward
  etna::GraphicsPipeline m_forwardPipeline {};

  // Deferred
  etna::GraphicsPipeline m_geometryPipeline {};
  std::unique_ptr<PostfxRenderer> m_pGbufferResolver {};

  // Shadow maps
  etna::GraphicsPipeline m_simpleShadowPipeline {};
  etna::GraphicsPipeline m_vsmShadowPipeline    {};
  etna::ComputePipeline  m_vsmFilteringPipeline {};

  // Anti-aliasing
  std::unique_ptr<PostfxRenderer> m_pTaaReprojector;
  
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VulkanSwapChain m_swapchain;

  Camera   m_cam;
  uint32_t m_width  = 1024u;
  uint32_t m_height = 1024u;
  uint32_t m_framesInFlight = 2u;
  bool m_vsync = false;

  vk::PhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char*> m_deviceExtensions;
  std::vector<const char*> m_instanceExtensions;

  std::shared_ptr<SceneManager> m_pScnMgr;
  std::shared_ptr<IRenderGUI> m_pGUIRender;
  
  std::unique_ptr<QuadRenderer> m_pQuad;
  
  enum ShadowmapTechnique
  {
    eShTechNone = 0,
    eSimple,
    eVsm,
    ePcf,
    // @TODO(PKiyashko): esm (someday)

    eShTechMax
  };
  // @TODO(PKiyashko): make use of special MSAA settings in AttacmentParams, provided graciously by Kostya
  // @TODO(PKiyashko): add different scale settings (xN)
  // @TODO(PKiyashko): proper taa with motion vectors and deferred
  enum AATechnique
  {
    eAATechNone = 0,
    eSsaa,
    eMsaa,
    eTaa,

    eAATechMax
  };

  // Forbidden combinations: msaa + deferred
  bool useDeferredRendering                    = true;
  ShadowmapTechnique currentShadowmapTechnique = eVsm;
  AATechnique currentAATechnique               = eSsaa;

  bool settingsAreDirty = true;

  struct InputControlMouseEtc
  {
    bool drawFSQuad = false;
  } m_input;

  /**
  \brief basic parameters that you usually need for shadow mapping
  */
  struct ShadowMapCam
  {
    ShadowMapCam() 
    {  
      cam.pos    = float3(4.0f, 4.0f, 4.0f);
      cam.lookAt = float3(0, 0, 0);
      cam.up     = float3(0, 1, 0);
  
      radius          = 5.0f;
      lightTargetDist = 20.0f;
      usePerspectiveM = true;
    }

    float  radius;           ///!< ignored when usePerspectiveM == true 
    float  lightTargetDist;  ///!< identify depth range
    Camera cam;              ///!< user control for light to later get light worldViewProj matrix
    bool   usePerspectiveM;  ///!< use perspective matrix if true and ortographics otherwise
  
  } m_light;
 
  void DrawFrame(bool draw_gui);
  void BuildCommandBuffer(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView);

  void LoadShaders();

  void RecreateSwapChain();

  void UpdateUniformBuffer(float a_time);

  void SetupDeviceExtensions();

  void AllocateResources();
  void PreparePipelines();

  void DeallocateResources();

  void InitPresentStuff();
  void ResetPresentStuff();

  void DoImGUI();
  void DeferredChoiceGUI();
  void ShadowmapChoiceGUI();
  void AAChoiceGui();

  // Common
  std::vector<etna::RenderTargetState::AttachmentParams> CurrentRTAttachments(
    VkImage a_targetImage, VkImageView a_targetImageView);
  etna::RenderTargetState::AttachmentParams CurrentRTDepthAttachment();
  vk::Rect2D CurrentRTRect();
  const char *CurrentRTProgramName();
  std::vector<std::vector<etna::Binding>> CurrentRTBindings();
  void RecordDrawSceneCmds(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout = VK_NULL_HANDLE);

  // Forward shading
  void LoadForwardShaders();
  void RebuildCurrentForwardPipeline();
  const char *CurrentForwardProgramName();
  GBuffer *CurrentGbuffer();
  void RecordForwardPassCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView);

  // Deferred shading
  void AllocateDeferredResources();
  void DeallocateDeferredResources();
  void LoadDeferredShaders();
  void SetupDeferredPipelines();
  void RebuildCurrentDeferredPipelines();
  const char *CurrentResolveProgramName();
  void RecordGeomPassCommands(VkCommandBuffer a_cmdBuff);
  void RecordResolvePassCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView);

  // Shadowmap techniques
  void AllocateShadowmapResources();
  void DeallocateShadowmapResources();
  void LoadShadowmapShaders();
  void SetupShadowmapPipelines();
  void RecordShadowPassCommands(VkCommandBuffer a_cmdBuff);
  void RecordShadowmapProcessingCommands(VkCommandBuffer a_cmdBuff);

  // AA techniques
  void AllocateAAResources();
  void DeallocateAAResources();
  void SetupAAPipelines();
  void RecordAAResolveCommands(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView);
  // @TODO(PKiyashko): this update should be less hacky, more centralized with others
  float CurrentTaaReprojectionCoeff();
};


#endif //CHIMERA_SIMPLE_RENDER_H
