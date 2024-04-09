#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_color;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout (binding = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D thisFrame;
layout (binding = 2) uniform sampler2D prevFrame;

layout (binding = 3) uniform sampler2D depthTex;

const vec2 jitterPhases[16] = 
{
  vec2(0.625, 0.375), vec2(0.625, 0.875), vec2(0.125, 0.375), vec2(0.875, 0.125),
  vec2(0.625, 0.625), vec2(0.125, 0.625), vec2(0.625, 0.125), vec2(0.875, 0.875),
  vec2(0.375, 0.625), vec2(0.375, 0.125), vec2(0.875, 0.625), vec2(0.125, 0.875),
  vec2(0.375, 0.375), vec2(0.875, 0.375), vec2(0.375, 0.875), vec2(0.125, 0.125),
};

void main()
{
  const float depth = textureLod(depthTex, surf.texCoord, 0).x;
  const vec4 posHom = inverse(Params.projViewMatrix) * vec4((2.0*surf.texCoord)-1.0, depth, 1.0);
  const vec4 pos = posHom / posHom.w;

  const vec4 reprojectedPos = Params.prevProjViewMatrix * pos;

  const vec2 thisFrameTexCoord = vec2(surf.texCoord.x, 1.0-surf.texCoord.y);
  const vec2 prevFrameTexCoord = 0.5*(vec2(reprojectedPos.x, -reprojectedPos.y) / reprojectedPos.w) + 0.5;
  const vec2 jitter = 0.6 * jitterPhases[Params.frameCounter % 16] / textureSize(prevFrame, 0);

  const vec4 colorThis = textureLod(thisFrame, thisFrameTexCoord, 0);
  const vec4 colorPrev = textureLod(prevFrame, prevFrameTexCoord + jitter, 0);

  out_color = colorThis * (1.0 - Params.reprojectionCoeff) + colorPrev * Params.reprojectionCoeff;
}
