#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 wUV;
  uint partId;
} vOut;

layout(location = 0) out vec4 out_fragColor;

layout(std430, binding = 2) buffer Particles 
{
  Particle particles[];
};

layout(binding = 0, set = 1) uniform sampler2D atlas;

void main(void)
{
  Particle part = particles[vOut.partId];

  // @HACK this should not be hardcoded (atlas layout)
  const uint atlas_w = 8;
  const uint atlas_h = 11;
  const uint num_frames = atlas_w * atlas_h;

  float ratio = clamp(part.timePhase / part.maxTime, 0.0, 1.0);
  
  uint lFrameId = min(uint(floor(ratio * float(num_frames))), num_frames - 1);
  uint rFrameId = lFrameId == num_frames - 1 ? lFrameId : lFrameId+1;
  float mixCoeff = fract(ratio * float(num_frames));

  vec2 lFramePos = vec2(lFrameId % atlas_w, lFrameId / atlas_w);
  vec2 rFramePos = vec2(rFrameId % atlas_w, rFrameId / atlas_w);

  vec2 atlasSize = vec2(atlas_w, atlas_h);
  vec2 lFrameCoord = (lFramePos + vOut.wUV) / atlasSize;
  vec2 rFrameCoord = (rFramePos + vOut.wUV) / atlasSize;

  out_fragColor = mix(
    texture(atlas, lFrameCoord),
    texture(atlas, rFrameCoord),
    mixCoeff);
}
