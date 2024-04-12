#include "shadowmap_render.h"

/// TECHNIQUE CHOICE & BUILDERS

std::vector<etna::RenderTargetState::AttachmentParams> SimpleShadowmapRender::CurrentRTAttachments(
  VkImage a_targetImage, VkImageView a_targetImageView)
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return {{ssaaRt.get(), ssaaRt.getView({})}};
  case eMsaa:
    return {{msaaRt.get(), msaaRt.getView({})}};
  case eTaa:
    return {{taaRt.get(), taaRt.getView({})}};
  default:
    return {{a_targetImage, a_targetImageView}};
  }
}

etna::RenderTargetState::AttachmentParams SimpleShadowmapRender::CurrentRTDepthAttachment()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return {ssaaDepth.get(), ssaaDepth.getView({})};
  case eMsaa:
    return {msaaDepth.get(), msaaDepth.getView({})};
  default:
    return {mainViewDepth.get(), mainViewDepth.getView({})};
  }
}

vk::Rect2D SimpleShadowmapRender::CurrentRTRect()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return vk::Rect2D{0, 0, m_width*2, m_height*2};
  default:
    return vk::Rect2D{0, 0, m_width, m_height};
  }
}

const char *SimpleShadowmapRender::CurrentRTProgramName()
{
  switch (currentShadowmapTechnique)
  {
  case eSimple:
    return useDeferredRendering ? "simple_resolve" : "simple_forward";
    break;
  case ePcf:
    return useDeferredRendering ? "pcf_resolve" :  "pcf_forward";
    break;
  case eVsm:
    return useDeferredRendering ? "vsm_resolve" : "vsm_forward";
    break;
  }
}

std::vector<std::vector<etna::Binding>> SimpleShadowmapRender::CurrentRTBindings()
{
  switch (currentShadowmapTechnique)
  {
  case eVsm:
  {
    return 
    {{
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, vsmSmoothMomentMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }
    }};
  } break;
  case eSimple: 
  case ePcf:
  {
    return 
    {{
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) } 
    }};
  } break;
  }

  return {{}};
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordDrawSceneCmds(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout)
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
