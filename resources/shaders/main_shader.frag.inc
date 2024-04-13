layout(location = 0) out vec4 out_fragColor;

#if USE_GBUFFER

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(set = 1, binding = 0) uniform sampler2D gNormal;
layout(set = 1, binding = 1) uniform sampler2D gDepth;

#define UNPACK_PARAMETERS()                                                                      \
  const vec2 _texCoord = vec2(surf.texCoord.x, 1.0 - surf.texCoord.y);                           \
  const vec3 norm = textureLod(gNormal, _texCoord, 0).xyz;                                       \
  const float _depth = textureLod(gDepth, _texCoord, 0).x;                                       \
  const vec4 _pos_hom = inverse(Params.projViewMatrix) * vec4((2.0*_texCoord)-1.0, _depth, 1.0); \
  const vec3 pos = (_pos_hom / _pos_hom.w).xyz

#else

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

#define UNPACK_PARAMETERS()     \
  const vec3 pos  = surf.wPos;  \
  const vec3 norm = surf.wNorm;

#endif
