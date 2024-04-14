#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, ccw) in;

layout(location = 0) out TE_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} teOut;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
} params;

layout(binding = 8) uniform sampler2D hmap;

void main()
{
  const float heightNorm = textureLod(hmap, gl_TessCoord.xy, 0).x;
  const vec3 posNorm = vec3(gl_TessCoord.x, heightNorm, gl_TessCoord.y);

  const float hx1  = textureOffset(hmap, gl_TessCoord.xy, ivec2(1.0, 0.0)).x;
  const float hxm1 = textureOffset(hmap, gl_TessCoord.xy, ivec2(-1.0, 0.0)).x;
  const float hz1  = textureOffset(hmap, gl_TessCoord.xy, ivec2(0.0, 1.0)).x;
  const float hzm1 = textureOffset(hmap, gl_TessCoord.xy, ivec2(0.0, -1.0)).x;

  const vec3 norm = normalize(cross(vec3(0.0, hz1 - hzm1, 2.0), vec3(2.0, hx1 - hxm1, 0.0)));
  
  teOut.wPos = (params.mModel * vec4(posNorm, 1.0)).xyz;
  teOut.wNorm = normalize(mat3(transpose(inverse(params.mModel))) * norm);
  // @NOTE(PKiyashko): we do not texture terrain now => these are stubs
  teOut.wTangent = vec3(0.0);
  teOut.texCoord = vec2(0.0);

  gl_Position = params.mProjView * vec4(teOut.wPos, 1.0);
}
