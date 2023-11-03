#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t {
    mat4 mProjView;
} params;

layout(location = 0) out VS_OUT {
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform AppData {
    UniformParams Params;
};

layout(std430, binding = 0, set = 1) readonly buffer InstanceData {
    mat4 instanceMatrices[];
};

layout(std430, binding = 1, set = 1) buffer InstanceIndices {
    uint counter;
    uint markedInstanceIndices[];
};

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    // @TEST
    if (markedInstanceIndices[gl_InstanceIndex] == 1)
        vOut.wPos = (instanceMatrices[gl_InstanceIndex] * vec4(vPosNorm.xyz, 1.0f)).xyz;
    else
        vOut.wPos = vec3(-100.0, -100.0, -100.0);
        //vOut.wPos = (params.mProjView * instanceMatrices[gl_InstanceIndex] * vec4(vPosNorm.xyz, 1.0f)).xyz;

    //vOut.wPos     = (instanceMatrices[gl_InstanceIndex] * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(instanceMatrices[gl_InstanceIndex]))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(instanceMatrices[gl_InstanceIndex]))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
