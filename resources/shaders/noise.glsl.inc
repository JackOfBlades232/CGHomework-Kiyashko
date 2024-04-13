#ifndef NOISE_GLSL_INC
#define NOISE_GLSL_INC

float simple_1d_noise(float key, float seed)
{
  vec3 a = 0.432312*vec3(fract(key * seed)) + vec3(0.11, -0.33, 0.03);
  float b = fract(dot(a, a)*a.x*a.y*a.z);
  return 0.97*b*b;
}

vec2 simple_2d_noise_derived(vec2 key, float seed)
{
  return vec2(
      simple_1d_noise(key.x, seed), 
      simple_1d_noise(simple_1d_noise(key.y, seed), seed));
}

float perlin_noise_2d(vec2 key, float seed)
{
  vec2 p1 = fract(key); 
  vec2 p2 = p1 + vec2(1.0, 0.0);
  vec2 p3 = p1 + vec2(0.0, 1.0);
  vec2 p4 = p1 + vec2(1.0, 1.0);

  vec2 g1 = normalize(simple_2d_noise_derived(p1, seed));
  vec2 g2 = normalize(simple_2d_noise_derived(p2, seed));
  vec2 g3 = normalize(simple_2d_noise_derived(p3, seed));
  vec2 g4 = normalize(simple_2d_noise_derived(p4, seed));

  float d1 = dot(g1, key - p1);
  float d2 = dot(g2, key - p2);
  float d3 = dot(g3, key - p3);
  float d4 = dot(g4, key - p4);

  // @TODO: smooth function?
  return (d1 + d2 + d3 + d4) * 0.25;
}

#endif
