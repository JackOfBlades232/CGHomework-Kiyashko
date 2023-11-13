#version 450
/*
#extension GL_GOOGLE_include_directive : require

#include "common.h"
*/

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

/*
layout(binding = 0, set = 0) uniform AppData
{
    UniformParams uParams;
};
*/

// Discarding tangent and texCoord since they are not used in frag shader
layout(location = 0) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut[];

layout(location = 0) out GS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} gOut;

void emit_triangle(int vi1, int vi2, vec3 other_pos, vec3 other_norm)
{
    gOut.wPos = vOut[vi1].wPos;
    gOut.wNorm = vOut[vi1].wNorm;
    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);
    EmitVertex();

    gOut.wPos = other_pos;
    gOut.wNorm = other_norm;
    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);
    EmitVertex();

    gOut.wPos = vOut[vi2].wPos;
    gOut.wNorm = vOut[vi2].wNorm;
    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);
    EmitVertex();

    EndPrimitive();
}

#define PYRAMID_HEIGHT 0.1

void main()
{
    for (int i = 0; i < gl_in.length() / 3; i++) {
        vec3 center      = (vOut[i].wPos + vOut[i+1].wPos + vOut[i+2].wPos)/3.0;
        vec3 center_norm = (vOut[i].wNorm + vOut[i+1].wNorm + vOut[i+2].wNorm)/3.0;

        vec3 pyramid_top = center + center_norm*PYRAMID_HEIGHT;

        // @TODO: make three distinct normals for top?
        emit_triangle(i,   i+2, pyramid_top, center_norm);
        emit_triangle(i+2, i+1, pyramid_top, center_norm);
        emit_triangle(i+1, i,   pyramid_top, center_norm);
    }
}
