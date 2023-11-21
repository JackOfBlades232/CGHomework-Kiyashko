#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(location = 0)  in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (binding = 1) uniform sampler2D gNormal;
layout (binding = 2) uniform sampler2D gDepth;

void main()
{
    const vec2 screenSize = vec2(textureSize(gNormal, 0));
    const vec2 screenCoord = gl_FragCoord.xy / screenSize;
    
    const vec3 normal = textureLod(gNormal, screenCoord, 0).xyz;
    const float depth = textureLod(gDepth, screenCoord, 0).x;
    const vec4 pos_hom = inverse(Params.projViewMatrix) * vec4((2.0*screenCoord)-1.0, depth, 1.0);
    const vec3 pos = (pos_hom / pos_hom.w).xyz;

    // @TODO: render volume light
    // @TEST
    out_fragColor = vec4((normal + vec3(1.0))*0.5, 1.0);
}
