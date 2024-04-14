import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = [
        "simple.vert", 
        "quad3_vert.vert", 
        "quad.frag", 
        "simple.frag", 
        "simple_shadow.frag", 
        "simple_gpass.frag", 
        "vsm_filter.comp",
        "vsm_shadow.frag", 
        "vsm_shadowmap.frag", 
        "pcf_shadow.frag", 
        "taa_simple.frag", 
        "generate_hmap.comp",
        "terrain.vert",
        "terrain.tesc",
        "terrain.tese",
        "generate_volfog.comp",
    ]

    # @TODO(PKiyashko): a more comprehensive naming rule
    deferred_resolve_shader_list = [
        "simple.frag", 
        "simple_shadow.frag", 
        "vsm_shadow.frag", 
        "pcf_shadow.frag", 
    ]
    deferred_resolve_output_list = [
        "simple_resolve.frag", 
        "shadow_resolve.frag", 
        "vsm_resolve.frag", 
        "pcf_resolve.frag", 
    ]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-DUSE_GBUFFER=0", "-o", "{}.spv".format(shader)])

    for shader, output in zip(deferred_resolve_shader_list, deferred_resolve_output_list):
        subprocess.run([glslang_cmd, "-V", shader, "-DUSE_GBUFFER=1", "-o", "{}.spv".format(output)])

