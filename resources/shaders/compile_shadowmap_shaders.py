import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "quad.vert", "quad.frag", "fullscreen_quad.vert", "simple_lpass.frag", "simple_gpass.frag"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

