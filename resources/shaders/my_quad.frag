#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D color_tex;
layout(location = 0) in VS_OUT { vec2 tex_coord; } surf;

// Bilateral filtering

const ivec2 window_size = ivec2(8);

const float spatial_var    = 0.1;
const float colorspace_var = 0.05;

const float spatial_norm_factor = length(vec2(window_size/2));

void main()
{
    vec4 base_color  = textureLod(color_tex, surf.tex_coord, 0);
    vec2 tex_size    = textureSize(color_tex, 0);
    ivec2 tex_icoord = ivec2(tex_size * surf.tex_coord);

    ivec2 window_bottom = max(tex_icoord - (window_size/2), ivec2(0));
    ivec2 window_top    = min(tex_icoord + (window_size/2), ivec2(tex_size));

    float w = 0.0;
    vec4 c  = vec4(vec3(0.0), 1.0);

    for (int y = window_bottom.y; y < window_top.y; y++)
        for (int x = window_bottom.x; x < window_top.x; x++) {
            ivec2 texel_icoord = ivec2(x, y);
            vec4 texel_color   = texelFetch(color_tex, texel_icoord, 0);

            float spat_dist       = distance(vec2(tex_icoord), vec2(texel_icoord)) / spatial_norm_factor;
            float colorspace_dist = distance(base_color, texel_color);

            float sd2 = spat_dist * spat_dist;
            float cd2 = colorspace_dist * colorspace_dist;

            float weight = exp(-sd2/(2.0*spatial_var) - cd2/(2.0*colorspace_var));

            w += weight;
            c += weight * texel_color;
        }

    color = c / w;
    color.a = 1.0;
}
