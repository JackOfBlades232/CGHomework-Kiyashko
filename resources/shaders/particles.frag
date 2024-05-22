#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 wUV;
  float ratio;
} vOut;

layout(location = 0) out vec4 out_fragColor;

// https://www.shadertoy.com/view/3tSGDy
float star( in vec2 p, in float r, in int n, in float m)
{
    // next 4 lines can be precomputed for a given shape
    float an = 3.141593/float(n);
    float en = 3.141593/m;  // m is between 2 and n
    vec2  acs = vec2(cos(an),sin(an));
    vec2  ecs = vec2(cos(en),sin(en)); // ecs=vec2(0,1) for regular polygon

    float bn = mod(atan(p.x,p.y),2.0*an) - an;
    p = length(p)*vec2(cos(bn),abs(sin(bn)));
    p -= r*acs;
    p += ecs*clamp( -dot(p,ecs), 0.0, r*acs.y/ecs.y);
    return length(p)*sign(p.x);
}

void main(void)
{
  float d = star(vOut.wUV*2.0 - 1.0, 0.5, 8, 4.0);
  if (d <= 0)
    out_fragColor = vec4(mix(vec3(1.0), vec3(0.8, 0.0, 0.8), -d), 1.0 - vOut.ratio);
  else
    out_fragColor = vec4(0.0);
}
