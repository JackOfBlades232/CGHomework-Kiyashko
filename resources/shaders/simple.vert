#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(location = 0) out VS_OUT
{
    vec3 mPos;
    vec3 mNorm;
    vec3 mTangent;
    vec2 texCoord;
} vOut;

void main(void)
{
    vOut.mPos     = vPosNorm.xyz;
    vOut.mNorm    = normalize(DecodeNormal(floatBitsToInt(vPosNorm.w)));
    vOut.mTangent = normalize(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)));
    vOut.texCoord = vTexCoordAndTang.xy;
}
