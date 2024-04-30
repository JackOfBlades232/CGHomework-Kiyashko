#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1) uniform sampler2D gDepth;

layout(binding = 2) uniform sampler2D rsmPositions;
layout(binding = 3) uniform sampler2D rsmNormals;
layout(binding = 4) uniform sampler2D rsmFlux;

layout(binding = 5) uniform SamplesDistribution
{
  vec4 samples[RSM_DISTIBUTION_SIZE]; // xy -- offset texcoord, z -- weight
};

void main()
{
  const float depth = textureLod(gDepth, surf.texCoord, 0).x;

  if (depth > 0.9999f)
  {
    out_fragColor = vec4(vec3(0.0), 1.0);
    return;
  }
  const vec4 posHom = inverse(Params.projViewMatrix) * vec4((2.0*surf.texCoord)-1.0, depth, 1.0);
  const vec3 pos = (posHom / posHom.w).xyz;

  const vec4 posLightClipSpace = Params.lightMatrix*vec4(pos, 1.0f);
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);

  const vec3 norm = textureLod(rsmNormals, shadowTexCoord, 0).xyz;

  vec3 e = vec3(0.f);
  for (int i = 0; i < RSM_DISTIBUTION_SIZE; ++i)
  {
    const vec2 sampleTexCoord = shadowTexCoord + samples[i].xy;
    const vec3 samplePosition = textureLod(rsmPositions, sampleTexCoord, 0).xyz;
    const vec3 sampleNorm = textureLod(rsmNormals, sampleTexCoord, 0).xyz;
    const vec3 sampleFlux = textureLod(rsmFlux, sampleTexCoord, 0).xyz;

    e += sampleFlux * 
         max(dot(sampleNorm, normalize(pos - samplePosition)), 0.f) *
         max(dot(norm, normalize(samplePosition - pos)), 0.f) *
         samples[i].z;
  }

  out_fragColor = vec4(e, 1.f);
}
