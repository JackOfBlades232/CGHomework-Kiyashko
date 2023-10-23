#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "common.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t {
    mat4 mProjView;
    mat4 mModel;
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

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    // @NOTE: the sines could be precalculated once on the CPU and passed once 
    //        with UniformParams, i know))
    vOut.wPos *= vec3(0.65) + vec3(0.05*sin(Params.time*3.5), 
                                  0.3*sin(Params.time*1.1), 
                                  -0.5*sin(Params.time*0.7));
    vOut.wPos += vec3(0.0, 0.35*sin(Params.time*2.5), 0.0);
    vOut.wPos += vec3(0.0, 0.05*sin(vOut.wPos.x*10.0 + Params.time*5.0), 0.0);

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
