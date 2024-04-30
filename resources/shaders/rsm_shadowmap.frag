#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_pos;
layout(location = 1) out vec4 out_norm;
layout(location = 2) out vec4 out_flux;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

void main()
{
  out_pos = vec4(surf.wPos, 1.f);
  out_norm = vec4(surf.wNorm, 1.f);

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);
  vec4 lightColor = mix(dark_violet, chartreuse, abs(sin(Params.time)));

  out_flux = Params.lightSourcesIntensityCoeff * lightColor;
}
