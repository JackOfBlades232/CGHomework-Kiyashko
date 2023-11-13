#version 450
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout(binding = 0) uniform AppData
{
    UniformParams uParams;
};

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

#define PYRAMID_HEIGHT_BASE 0.1
#define PYRAMID_HEIGHT_AMP  0.075
#define UV_MAX              0.7
#define SUB_PERIOD          0.4

void main()
{
    float pyramid_height = PYRAMID_HEIGHT_BASE;
    vec3 w               = vec3(1.0/3.0);

    float t = mod(uParams.time, SUB_PERIOD*8.0);

    if (t >= SUB_PERIOD*6.0) {
        float t_in = mod(t + SUB_PERIOD*0.5, SUB_PERIOD*2.0);
        float coeff_base = abs(1.0 - (t_in/SUB_PERIOD));
        pyramid_height += PYRAMID_HEIGHT_AMP * (0.5-coeff_base);
    } else {
        float t_in = mod(t, SUB_PERIOD*2.0);
        float coeff_base = 1.0 - abs(1.0 - (t_in/SUB_PERIOD));

        float main_coeff = min(1.0/3.0 + coeff_base*(UV_MAX - 1.0/3.0), 1.0);
        float side_coeff = (1.0 - main_coeff) * 0.5;

        w = vec3(side_coeff);

        if (t >= SUB_PERIOD*4.0)
            w.z = main_coeff;
        else if (t >= SUB_PERIOD*2.0)
            w.y = main_coeff;
        else
            w.x = main_coeff;
    }

    for (int i = 0; i < gl_in.length() / 3; i++) {
        fill_tri_buffer(i);

        vec3 center      = w.x * tri_buffer[0].wPos + w.y * tri_buffer[1].wPos + w.z * tri_buffer[2].wPos;
        vec3 center_norm = w.x * tri_buffer[0].wNorm + w.y * tri_buffer[1].wNorm + w.z * tri_buffer[2].wNorm;
        vec3 pyramid_top = center + center_norm*pyramid_height;

        emit_triangle(0, 2, pyramid_top, avg_norm(0, 2));
        emit_triangle(2, 1, pyramid_top, avg_norm(2, 1));
        emit_triangle(1, 0, pyramid_top, avg_norm(1, 0));
    }
}
