#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(vertices = 4) out;

void main()
{
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

  if (gl_InvocationID == 0)
  {
    gl_TessLevelOuter[0] = LANDMESH_DIM;
    gl_TessLevelOuter[1] = LANDMESH_DIM;
    gl_TessLevelOuter[2] = LANDMESH_DIM;
    gl_TessLevelOuter[3] = LANDMESH_DIM;

    gl_TessLevelInner[0] = LANDMESH_DIM;
    gl_TessLevelInner[1] = LANDMESH_DIM;
  }
}
