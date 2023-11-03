#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t {
    mat4 mProjView;
    uint mModelIndex;
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

layout(std140, binding = 0, set = 1) readonly buffer InstanceData {
    mat4 instanceMatrices[];
};

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (instanceMatrices[params.mModelIndex] * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(instanceMatrices[params.mModelIndex]))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(instanceMatrices[params.mModelIndex]))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;
    /*
    vOut.wPos     = (instanceMatrices[26] * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(instanceMatrices[26]))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(instanceMatrices[26]))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;
    */

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
