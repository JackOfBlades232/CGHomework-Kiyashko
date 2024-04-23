import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = [
        "simple.vert", 
        "quad3_vert.vert", 
        "quad.frag", 
        "simple_gpass.frag", 
        "vsm_filter.comp",
        "vsm_shadowmap.frag", 
        "taa_simple.frag", 
        "generate_hmap.comp",
        "terrain.vert",
        "terrain.tesc",
        "terrain.tese",
        "generate_volfog.comp",
        "apply_volfog.frag",
        "generate_ssao.frag", 
        "blur_ssao.comp", 
        "simple.frag", 
        "simple_shadow.frag", 
        "vsm_shadow.frag", 
        "pcf_shadow.frag", 
        "tonemapping.frag", 
    ]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-DUSE_DEFERRED=1", "-o", "{}.spv".format(shader)])

