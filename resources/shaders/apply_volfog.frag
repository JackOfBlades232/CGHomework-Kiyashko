#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D colorTex;
layout(binding = 1) uniform sampler2D volfogMap;

layout(location = 0) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
{
  const vec4 color = textureLod(colorTex, surf.texCoord, 0);
  const vec4 fogAmt = textureLod(volfogMap, surf.texCoord, 0);

  out_color = mix(color, vec4(0.25, 0.25, 0.25, 1.0), fogAmt);
}
