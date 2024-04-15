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
  class RenderTarget
  {
    etna::Image im[2];
    etna::Image *currentIm;
    
  public:
    RenderTarget() : currentIm(&im[0]) {}
    RenderTarget(etna::Image::CreateInfo imCreateInfo, etna::GlobalContext *ctx) : currentIm(&im[0])
      { im[0] = ctx->createImage(imCreateInfo); im[1] = ctx->createImage(imCreateInfo); }

    RenderTarget(RenderTarget &&other) noexcept 
      { im[0] = std::move(other.im[0]); im[1] = std::move(other.im[1]); other.currentIm = nullptr; }
    RenderTarget& operator=(RenderTarget &&other) noexcept 
      { im[0] = std::move(other.im[0]); im[1] = std::move(other.im[1]); other.currentIm = nullptr; return *this; }

    ~RenderTarget() { reset(); }

    void flip() { currentIm = currentIm == &im[0] ? &im[1] : &im[0]; }
    void reset() { im[0].reset(); im[1].reset(); }
    etna::Image &current() { return *currentIm; }
    etna::Image &backup() { return currentIm == &im[0] ? im[1] : im[0]; }
  };

  etna::GlobalContext* m_context;

  RenderTarget mainRt;

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
  // @TODO(PKiyashko): this is a lot of excess resources. Maybe recreate instead?
  etna::Image ssaaFrame;
  etna::Image ssaaDepth;
  GBuffer ssaaGbuffer;

  etna::Image taaFrames[2];
  etna::Image *taaCurFrame = &taaFrames[0];
  etna::Image *taaPrevFrame = &taaFrames[1];

  // Terrain
  etna::Image terrainHmap;

  // Volfog
  etna::Image volfogMap;

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

  // Terrain
  etna::GraphicsPipeline m_terrainForwardPipeline, m_terrainGpassPipeline;
  etna::GraphicsPipeline m_terrainSimpleShadowPipeline, m_terrainVsmPipeline;
  etna::ComputePipeline m_hmapGeneratePipeline;

  // Volfog
  etna::ComputePipeline m_volfogGeneratePipeline;
  std::unique_ptr<PostfxRenderer> m_pVolfogApplier;

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
  // @TODO(PKiyashko): make use of special MSAA settings in AttacmentParams, provided graciously by Kostya?
  // @TODO(PKiyashko): add different scale settings (xN)
  // @TODO(PKiyashko): proper taa with motion vectors and deferred
  // @TODO(PKiyashko): Add back msaa with proper depth resolve for postfx and masks for deferred
  enum AATechnique
  {
    eAATechNone = 0,
    eSsaa,
    eTaa,

    eAATechMax
  };

  // @TODO(PKiyashko): forward & deferred look a bit different (overall tone), should check out

  bool useDeferredRendering                    = true;
  ShadowmapTechnique currentShadowmapTechnique = eVsm;
  AATechnique currentAATechnique               = eSsaa;
  bool volfogEnabled                           = true;

  bool needToRegenerateHmap  = true;
  float2 terrainMinMaxHeight = float2(0.f, 3.f);
  float3 windVelocity        = float3(-0.5f, 0.f, -0.5f);

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
  std::vector<etna::RenderTargetState::AttachmentParams> CurrentRTAttachments();
  etna::RenderTargetState::AttachmentParams CurrentRTDepthAttachment();
  etna::Image &GetCurrentResolvedDepthBuffer();
  vk::Rect2D CurrentRTRect();
  const char *CurrentRTProgramName();
  std::vector<std::vector<etna::Binding>> CurrentRTBindings();
  void RecordDrawSceneCommands(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout = VK_NULL_HANDLE);
  void BlitToTarget(
    VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView,
    etna::Image &rt, vk::Extent2D extent, VkFilter filter, 
    vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor);
  void BlitMainRTToScreen(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView) 
    { BlitToTarget(a_cmdBuff, a_targetImage, a_targetImageView, mainRt.current(), vk::Extent2D{ m_width, m_height }, VK_FILTER_NEAREST); }

  // Forward shading
  void LoadForwardShaders();
  void RebuildCurrentForwardPipelines();
  const char *CurrentForwardProgramName();
  GBuffer *CurrentGbuffer();
  void RecordForwardPassCommands(VkCommandBuffer a_cmdBuff);

  // Deferred shading
  void AllocateDeferredResources();
  void DeallocateDeferredResources();
  void LoadDeferredShaders();
  void SetupDeferredPipelines();
  void RebuildCurrentDeferredPipelines();
  const char *CurrentResolveProgramName();
  void RecordGeomPassCommands(VkCommandBuffer a_cmdBuff);
  void RecordResolvePassCommands(VkCommandBuffer a_cmdBuff);

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
  void RecordAAResolveCommands(VkCommandBuffer a_cmdBuff);
  // @TODO(PKiyashko): this update should be less hacky, more centralized with others
  float CurrentTaaReprojectionCoeff();

  // Terrain
  void AllocateTerrainResources();
  void DeallocateTerrainResources();
  void LoadTerrainShaders();
  void SetupTerrainPipelines();
  const char *CurrentTerrainForwardProgramName();
  float4x4 GetCurrentTerrainQuadTransform();
  void RecordHmapGenerationCommands(VkCommandBuffer a_cmdBuff);
  void RecordDrawTerrainForwardCommands(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp);
  void RecordDrawTerrainGpassCommands(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp);
  void RecordDrawTerrainToShadowmapCommands(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp);

  // Volfog
  void AllocateVolfogResources();
  void DeallocateVolfogResources();
  void LoadVolfogShaders();
  void SetupVolfogPipelines();
  void ReallocateVolfogResources() { AllocateVolfogResources(); }
  void RecordVolfogCommands(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp);
};


#endif //CHIMERA_SIMPLE_RENDER_H
