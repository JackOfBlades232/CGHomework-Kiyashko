import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = [
        "simple.vert", 
        "quad.vert", 
        "quad.frag", 
        "simple_shadow.frag", 
        "vsm_filter.comp",
        "vsm_shadow.frag", 
        "vsm_shadowmap.frag", 
        "pcf_shadow.frag", 
    ]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

