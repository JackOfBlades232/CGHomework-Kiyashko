#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out float out_fragColor; // @TODO: 16 unorm?

layout (location = 0 ) in VS_OUT
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

#define PI 3.14159265
#define DEG2RAD(_deg) ((_deg) * PI / 180.0)

void main()
{
    vec3 l = normalize(surf.wPos - Params.lightPos);
    vec3 d = normalize(Params.lightDir); // @TODO: do on cpu?
    float angle = acos(dot(l, d));
    out_fragColor = 1.0 - clamp((angle - Params.lightAngleInner) / (Params.lightAngleOuter - Params.lightAngleInner), 0.0, 1.0);
}
