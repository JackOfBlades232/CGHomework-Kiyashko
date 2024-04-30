#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "face_color.frag.inc"

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

  vec4 lightColor = vec4(0.5f, 0.5f, 0.5f, 1.f);
  vec4 meshCol = color_from_normal(surf.wNorm);

  out_flux = 3.f * Params.lightSourcesIntensityCoeff * lightColor * meshCol * vec4(Params.baseColor, 1.f);
}
