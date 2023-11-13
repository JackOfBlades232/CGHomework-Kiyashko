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
    vec3 mPos;
    vec3 mNorm;
    vec3 mTangent;
    vec2 texCoord;
} vOut[];

layout(location = 0) out GS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} gOut;

struct
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} tri_buffer[3];

void fill_tri_buffer(int base_id)
{
    for (int i = 0; i < 3; i++) {
        tri_buffer[i].wPos     = (params.mModel * vec4(vOut[base_id+i].mPos.xyz, 1.0f)).xyz;
        tri_buffer[i].wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * vOut[base_id+i].mNorm.xyz);
        /*
        tri_buffer[i].wTangent = normalize(mat3(transpose(inverse(params.mModel))) * vOut[base_id+i].mTangent.xyz);
        tri_buffer[i].texCoord = vOut[base_id+i].texCoord;
        */
    }
}

vec3 avg_norm(int vi1, int vi2)
{
    return (tri_buffer[vi1].wNorm + tri_buffer[vi2].wNorm) * 0.5;
}

void emit_triangle(int vi1, int vi2, vec3 other_pos, vec3 other_norm)
{
    gOut.wPos = tri_buffer[vi1].wPos;
    gOut.wNorm = tri_buffer[vi1].wNorm;
    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);
    EmitVertex();

    gOut.wPos = other_pos;
    gOut.wNorm = other_norm;
    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);
    EmitVertex();

    gOut.wPos = tri_buffer[vi2].wPos;
    gOut.wNorm = tri_buffer[vi2].wNorm;
    gl_Position = params.mProjView * vec4(gOut.wPos, 1.0);
    EmitVertex();

    EndPrimitive();
}

#define PYRAMID_HEIGHT 0.1

void main()
{
    for (int i = 0; i < gl_in.length() / 3; i++) {
        fill_tri_buffer(i);

        vec3 center      = (tri_buffer[0].wPos + tri_buffer[1].wPos + tri_buffer[2].wPos)/3.0;
        vec3 center_norm = (tri_buffer[0].wNorm + tri_buffer[1].wNorm + tri_buffer[2].wNorm)/3.0;
        vec3 pyramid_top = center + center_norm*PYRAMID_HEIGHT;

        // @TODO: make three distinct normals for top?
        emit_triangle(0, 2, pyramid_top, avg_norm(0, 2));
        emit_triangle(2, 1, pyramid_top, avg_norm(2, 1));
        emit_triangle(1, 0, pyramid_top, avg_norm(1, 0));
    }
}
