#include "shadowmap_render.h"

/// TECHNIQUE CHOICE & BUILDERS

std::vector<etna::RenderTargetState::AttachmentParams> SimpleShadowmapRender::CurrentRTAttachments()
{
  switch (currentAATechnique)
  {
  case eSsaa:
    return {{ssaaFrame.get(), ssaaFrame.getView({})}};
  case eTaa:
  default:
    return {{mainRt.current().get(), mainRt.current().getView({})}};
  }
}

etna::RenderTargetState::AttachmentParams SimpleShadowmapRender::CurrentRTDepthAttachment()
{
  if (useDeferredRendering)
    return {};
  else
  {
    switch (currentAATechnique)
    {
    case eSsaa:
      return {ssaaDepth.get(), ssaaDepth.getView({})};
    default:
      return {mainViewDepth.get(), mainViewDepth.getView({})};
    }
  }
}

etna::Image &SimpleShadowmapRender::GetCurrentResolvedDepthBuffer()
{
  if (useDeferredRendering)
    return *(CurrentGbuffer()->depth);
  else if (currentAATechnique == eSsaa)
    return ssaaDepth;
  else // MSAA resolves depth to mainViewDepth
    return mainViewDepth;
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
  return useDeferredRendering ? CurrentResolveProgramName() : CurrentForwardProgramName();
}

std::vector<std::vector<etna::Binding>> SimpleShadowmapRender::CurrentRTBindings()
{
  switch (currentShadowmapTechnique)
  {
  case eShTechNone:
    return {{ etna::Binding{0, constants.genBinding()} }};
    break;

  case eSimple: 
  case ePcf:
  {
    return 
    {{
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) } 
    }};
  } break;

  case eVsm:
  {
    return 
    {{
      etna::Binding{ 0, constants.genBinding() }, 
      etna::Binding{ 1, vsmSmoothMomentMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }
    }};
  } break;
  }

  return {};
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::RecordDrawSceneCommands(
  VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout)
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

void SimpleShadowmapRender::BlitToTarget(
    VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView,
    etna::Image &rt, vk::Extent2D extent, VkFilter filter, 
    vk::ImageAspectFlags aspectFlags)
{
  etna::set_state(a_cmdBuff, rt.get(), 
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlags2(vk::AccessFlagBits2::eTransferRead),
    vk::ImageLayout::eTransferSrcOptimal,
    aspectFlags);
  etna::set_state(a_cmdBuff, a_targetImage, 
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlags2(vk::AccessFlagBits2::eTransferWrite),
    vk::ImageLayout::eTransferDstOptimal,
    aspectFlags);
  etna::flush_barriers(a_cmdBuff);

  VkImageBlit blit;
  blit.srcSubresource.aspectMask     = (VkImageAspectFlags)aspectFlags;
  blit.srcSubresource.mipLevel       = 0;
  blit.srcSubresource.baseArrayLayer = 0;
  blit.srcSubresource.layerCount     = 1;
  blit.srcOffsets[0]                 = { 0, 0, 0 };
  blit.srcOffsets[1]                 = { (int32_t)extent.width, (int32_t)extent.height, 1 };
  blit.dstSubresource.aspectMask     = (VkImageAspectFlags)aspectFlags;
  blit.dstSubresource.mipLevel       = 0;
  blit.dstSubresource.baseArrayLayer = 0;
  blit.dstSubresource.layerCount     = 1;
  blit.dstOffsets[0]                 = { 0, 0, 0 };
  blit.dstOffsets[1]                 = { (int32_t)m_width, (int32_t)m_width, 1 };

  vkCmdBlitImage(
    a_cmdBuff,
    rt.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    a_targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &blit,
    filter);
}
