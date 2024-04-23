#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_color;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1) uniform sampler2D hdrTex;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
{
  const vec2 flippedTexCoord = vec2(surf.texCoord.x, 1.0-surf.texCoord.y);
  const vec3 hdrColor = textureLod(hdrTex, flippedTexCoord, 0).xyz;

  if (Params.tonemappingTechnique == REINHARD_TONE)
    out_color = vec4(pow(hdrColor / (vec3(1.0) + hdrColor), vec3(1.0/2.2)), 1.0);
  else if (Params.tonemappingTechnique == EXPOSURE_TONE)
    out_color = vec4(pow(vec3(1.0) - exp(-hdrColor * Params.exposureCoeff), vec3(1.0/2.2)), 1.0);
  else // None
    out_color = vec4(hdrColor, 1.0); // this will be clamped automatically
}
