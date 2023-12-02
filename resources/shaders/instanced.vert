#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
} params;

layout(location = 0) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
} vOut;

layout(std430, binding = 0) readonly buffer InstanceData {
    mat4 instanceMatrices[];
};

layout(std430, binding = 1) readonly buffer InstanceIndices {
    uint markedInstIndices[];
};

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);

    mat4 mModel = instanceMatrices[markedInstIndices[gl_InstanceIndex]];

    vOut.wPos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
