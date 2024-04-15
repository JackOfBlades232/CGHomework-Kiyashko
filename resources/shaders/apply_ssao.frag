#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1) uniform sampler2D colorTex;
layout(binding = 2) uniform sampler2D depthTex;

const float sphereRad = 0.1;
const float offsetMax = 0.02;

const vec3 baseSsaoSamplePoints[14] = 
  {
    vec3(0.66,  0.0,   0.0  ),
    vec3(-0.66, 0.0,   0.0  ),
    vec3(0.0,   0.66,  0.0  ),
    vec3(0.0,   -0.66, 0.0  ),
    vec3(0.0,   0.0,   0.66 ),
    vec3(0.0,   0.0,   -0.66),
    vec3(0.5,   0.5,   0.5  ),
    vec3(0.5,   0.5,   -0.5 ),
    vec3(0.5,   -0.5,  0.5  ),
    vec3(0.5,   -0.5,  -0.5 ),
    vec3(-0.5,  0.5,   0.5  ),
    vec3(-0.5,  0.5,   -0.5 ),
    vec3(-0.5,  -0.5,  0.5  ),
    vec3(-0.5,  -0.5,  -0.5 ),
  };

vec3 randomOffset(vec3 p) 
{
    return fract(sin(dot(p, vec3(12.9898, 78.233, 0.89301))*p) * 43758.5453123);
}

void main()
{
  const vec2 flippedTexCoord = vec2(surf.texCoord.x, 1.0-surf.texCoord.y);
  const float depth = textureLod(depthTex, flippedTexCoord, 0).x;
  const vec4 posHom = inverse(Params.projViewMatrix) * vec4((2.0*surf.texCoord)-1.0, depth, 1.0); \
  const vec3 pos = (posHom / posHom.w).xyz;

  int cnt = 0;
  for (int i = 0; i < 14; ++i)
  {
    vec3 coord = pos + baseSsaoSamplePoints[i] * sphereRad;
    coord += randomOffset(coord) * offsetMax;

    const vec4 ndcHom = Params.projViewMatrix * vec4(coord, 1.0); 
    const vec3 ndc = (ndcHom / ndcHom.w).xyz;

    const vec2 clipSpace = ndc.xy*0.5 + 0.5;
    const float depth = textureLod(depthTex, vec2(clipSpace.x, 1.0 - clipSpace.y), 0);

    if (ndc.z - depth > 0.0001f)
        ++cnt;
  }

  const float occ = float(cnt) / 14.0;
  out_fragColor = occ * texCoord
}
