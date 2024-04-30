#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

#include "main_shader.frag.inc"
#include "face_color.frag.inc"

void main()
{
  UNPACK_PARAMETERS();

  vec3 lightDir1 = normalize(Params.lightPos - pos);
  vec3 lightDir2 = vec3(0.0f, 0.0f, 1.0f);

  vec4 lightColor1 = vec4(0.5f, 0.5f, 0.5f, 1.f);
  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  vec3 N = norm; 

  vec4 color1 = max(dot(N, lightDir1), 0.0f) * lightColor1;
  vec4 color2 = max(dot(N, lightDir2), 0.0f) * lightColor2;
  vec4 color_lights = mix(color1, color2, 0.2f);

  vec4 lightColor  = color_lights * Params.lightSourcesIntensityCoeff;
  vec4 ambientColor = ambient * Params.ambientIntensityCoeff;

  out_fragColor = (lightColor + ambientColor) * vec4(Params.baseColor, 1.0f) * color_from_normal(norm) + indirectLightCol;
}
