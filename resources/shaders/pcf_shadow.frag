#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

#include "main_shader.frag.inc"
#include "face_color.frag.inc"

layout(binding = 1) uniform sampler2D shadowMap;

// This could've been pulled out...
void main()
{
  UNPACK_PARAMETERS();

  const vec4 posLightClipSpace = Params.lightMatrix*vec4(pos, 1.0f); // 
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]               

  const bool outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  float shadow = 1.0f;
  if (!outOfView) {
    vec2 texSize = textureSize(shadowMap, 0);
    float hitCount = 0.0f;
    float divisor = (2.0*float(VSM_WINDOW_HALFSIZE) + 1.0)*(2.0*float(VSM_WINDOW_HALFSIZE) + 1.0);
    vec2 pix_coord = shadowTexCoord * texSize;

    for (int y = int(pix_coord.y) - VSM_WINDOW_HALFSIZE; y < int(pix_coord.y) + VSM_WINDOW_HALFSIZE + 1; ++y)
      for (int x = int(pix_coord.x) - VSM_WINDOW_HALFSIZE; x < int(pix_coord.x) + VSM_WINDOW_HALFSIZE + 1; ++x) {
        if (textureLod(shadowMap, vec2(x, y)/texSize, 0).x + 0.001f > posLightSpaceNDC.z)
          hitCount += 1.0f;
      }

    shadow = hitCount/divisor;
  }

  vec4 lightColor1 = vec4(0.5f, 0.5f, 0.5f, 1.f);
  vec3 lightDir    = normalize(Params.lightPos - pos);

  vec4 lightColor  = max(dot(norm, lightDir), 0.0f) * lightColor1 * Params.lightSourcesIntensityCoeff;
  vec4 ambientColor = ambient * Params.ambientIntensityCoeff;

  out_fragColor = (lightColor*shadow + ambientColor) * vec4(Params.baseColor, 1.0f) * color_from_normal(norm) + indirectLightCol;
}
