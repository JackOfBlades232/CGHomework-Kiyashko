#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform params_t
{
    mat4 mProjView;
} params;

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 wUV;
  uint partId;
} vOut;

layout(std430, binding = 0) readonly buffer Matrices 
{
  mat4 particleMatrices[];
};

layout(std430, binding = 1) readonly buffer Indices 
{
  uint liveParticles;
  uint particleIndices[];
};

out gl_PerVertex { vec4 gl_Position; };

void main(void)
{
  vec3 basePos;
  if (gl_VertexIndex == 0 || gl_VertexIndex == 3)
  {
    basePos = vec3(-1.0, -1.0, 0.0);
    vOut.wUV = vec2(0.0);
  }
  else if (gl_VertexIndex == 1 || gl_VertexIndex == 5)
  {
    basePos = vec3(1.0, 1.0, 0.0);
    vOut.wUV = vec2(1.0);
  }
  else if (gl_VertexIndex == 2)
  {
    basePos = vec3(-1.0, 1.0, 0.0);
    vOut.wUV = vec2(0.0, 1.0);
  }
  else
  {
    basePos = vec3(1.0, -1.0, 0.0);
    vOut.wUV = vec2(1.0, 0.0);
  }
  vec3 baseNorm = vec3(0.0, 0.0, -1.0);

  vOut.partId = particleIndices[gl_InstanceIndex];
  mat4 mModel = particleMatrices[vOut.partId];

  vOut.wPos  = (mModel * vec4(basePos, 1.0f)).xyz;
  vOut.wNorm = normalize(mat3(transpose(inverse(mModel))) * baseNorm);

  gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
