#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout(location = 0) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
} vOut;

layout(binding = 0, std430) buffer InstanceMatrices
{
    mat4 instMatrices[];
};

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);

    // @HACK
    mat4 mModel;
    if (params.mModel[3][3] != 1.0)
        mModel = instMatrices[gl_InstanceIndex];
    else
        mModel = params.mModel;

    vOut.wPos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
