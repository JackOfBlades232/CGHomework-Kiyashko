#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
} params;

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 8, set = 0) uniform sampler2D heightMap;

void main()
{
  const vec2 base = vec2(-float(LANDMESH_DIM) * 0.5);
  const vec2 disp = vec2(1.0);
  const uint quadId = gl_VertexIndex / 6;
  const uint inQuadId = gl_VertexIndex % 6;
  const vec2 quadCoord = vec2(quadId % LANDMESH_DIM, quadId / LANDMESH_DIM);

  vec2 planarPos = base + disp * quadCoord;
  switch (inQuadId)
  {
  case 1:
    planarPos.x += 1.0;
    break;
  case 2:
  case 4:
    planarPos += vec2(1.0);
    break;
  case 5:
    planarPos.y += 1.0;
    break;
  }

  const float heightFrac = 
    textureLod(heightMap, (planarPos + base) / float(LANDMESH_DIM), 0).x;

  const vec3 pos = 
    vec3(planarPos.x, mix(Params.minMaxHeight.x, Params.minMaxHeight.y, heightFrac), planarPos.y);
  const vec3 norm = vec3(0.0, 1.0, 0.0);

  vOut.wPos     = (params.mModel * vec4(pos, 1.0f)).xyz;
  vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * norm);
  // @FEATURE(PKiyashko): this is a stub, implement these if there are textures
  vOut.wTangent = vec3(0.0);
  vOut.texCoord = vec2(0.0);

  gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
