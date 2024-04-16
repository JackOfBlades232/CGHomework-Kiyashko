#ifndef VK_GRAPHICS_BASIC_COMMON_H
#define VK_GRAPHICS_BASIC_COMMON_H

// GLSL-C++ datatype compatibility layer

#define F_PI 3.14159265359f

#define VSM_WORK_GROUP_DIM 16
#define VSM_WINDOW_HALFSIZE 5

#define HMAP_WORK_GROUP_DIM 16
#define LANDMESH_DIM 1000

#define VOLFOG_WORK_GROUP_DIM 16

#define SSAO_KERNEL_SIZE 64
#define SSAO_TANGENTS_DIM 4
#define SSAO_WINDOW_HALFSIZE 2
#define SSAO_WORK_GROUP_DIM 16

#ifdef __cplusplus

#include <LiteMath.h>

#include <cassert>
static_assert(VSM_WINDOW_HALFSIZE <= VSM_WORK_GROUP_DIM); // for compute shader
static_assert(SSAO_WINDOW_HALFSIZE <= SSAO_WORK_GROUP_DIM); // for compute shader

// NOTE: This is technically completely wrong,
// as GLSL words are guaranteed to be 32-bit,
// while C++ unsigned int can be 16-bit.
// Touching LiteMath is forbidden though, so yeah.
using shader_uint  = LiteMath::uint;
using shader_uvec2 = LiteMath::uint2;
using shader_uvec3 = LiteMath::uint3;

using shader_float = float;
using shader_vec2  = LiteMath::float2;
using shader_vec3  = LiteMath::float3;
using shader_vec4  = LiteMath::float4;
using shader_mat4  = LiteMath::float4x4;

// The funny thing is, on a GPU, you might as well consider
// a single byte to be 32 bits, because nothing can be smaller
// than 32 bits, so a bool has to be 32 bits as well.
using shader_bool  = LiteMath::uint;

#else

#define shader_uint  uint
#define shader_uvec2 uvec2

#define shader_float float
#define shader_vec2  vec2
#define shader_vec3  vec3
#define shader_vec4  vec4
#define shader_mat4  mat4

#define shader_bool  bool

#endif


// @TODO(PKiyashko): unify all places where matrices are used as push_constants and
//                   from this ubo
struct UniformParams
{
  shader_mat4  projViewMatrix;
  shader_mat4  projMatrix;
  shader_mat4  lightMatrix;
  shader_vec3  lightPos;
  shader_float time;
  shader_vec3  baseColor;
  shader_bool  animateLightColor;
  shader_mat4  prevProjViewMatrix;
  shader_vec3  windVel;
  shader_uint  frameCounter;
  shader_vec4  ambientLightIntensity;
  shader_float reprojectionCoeff;
  shader_float lightSourcesIntensityCoeff;
  shader_bool  useSsao;
};

struct SSAOUniformParams
{
  shader_vec4 kernel[SSAO_KERNEL_SIZE];
  shader_vec4 tangentBases[SSAO_TANGENTS_DIM*SSAO_TANGENTS_DIM];
};

#endif // VK_GRAPHICS_BASIC_COMMON_H
