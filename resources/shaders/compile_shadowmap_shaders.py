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
        "subsurface_scatter.comp",
        "particles_emit.comp",
        "particles_simulate.comp",
        "particles_sort.comp",
        "particles.vert",
        "particles.frag",
    ]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

