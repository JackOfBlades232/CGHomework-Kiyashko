#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out float out_ao;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1, set = 0) uniform SsaoData
{
  SSAOUniformParams ssaoParams;
};

layout(binding = 2) uniform sampler2D gDepth;
layout(binding = 3) uniform sampler2D gNormal;

const float aoRad = 0.1;
const float depthBias = 0.0025;

void main()
{
  const mat4 invProj = inverse(Params.projMatrix);

  const vec2 texCoord = vec2(surf.texCoord.x, 1.0 - surf.texCoord.y);
  const float depth = textureLod(gDepth, texCoord, 0).x;
  const vec4 posHom = invProj * vec4((2.0*texCoord)-1.0, depth, 1.0);
  const vec3 pos    = (posHom / posHom.w).xyz;
  const vec3 norm   = textureLod(gNormal, texCoord, 0).xyz;

  const uvec2 tangBaseCoord =
    uvec2(floor(fract(texCoord * textureSize(gDepth, 0) / float(SSAO_TANGENTS_DIM)) * float(SSAO_TANGENTS_DIM)));
  const vec3 rvec = ssaoParams.tangentBases[(tangBaseCoord.y * SSAO_TANGENTS_DIM) + tangBaseCoord.x].xyz;

  const vec3 tang   = normalize(rvec - norm * dot(rvec, norm));
  const vec3 bitang = cross(norm, tang);

  const mat3 tbn = mat3(tang, bitang, norm);

  float ao = 0.0;
  for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
  {
    const vec3 samplePos = pos + tbn * ssaoParams.kernel[i].xyz * aoRad;

    const vec4 sampleNdcHom = Params.projMatrix * vec4(samplePos, 1.0);
    const vec3 sampleNdc = (sampleNdcHom / sampleNdcHom.w).xyz;
    
    const float sampleZBufDepth = textureLod(gDepth, 0.5*sampleNdc.xy + 0.5, 0).x;
    const vec4 samplePosHom = invProj * vec4(sampleNdc.xy, sampleZBufDepth, 1.0);
    const float sampleDepth = (samplePosHom / samplePosHom.w).z;

    const float depthStep = smoothstep(0.0, 1.0, aoRad / abs(samplePos.z - sampleDepth));
    ao += (samplePos.z - sampleDepth <= depthBias ? 1.0 : 0.0) * depthStep;
  }

  out_ao = 1.0 - ao / float(SSAO_KERNEL_SIZE);
}
