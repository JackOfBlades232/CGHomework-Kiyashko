#ifndef NOISE_GLSL_INC
#define NOISE_GLSL_INC

// Thx to Ian McEwan
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }

float snoise(vec2 v)
{
  const vec4 C = vec4(0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439);

  vec2 i  = floor(v + dot(v, C.yy));
  vec2 x0 = v - i + dot(i, C.xx);

  vec2 i1 = vec2(0.0);
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec2 x1 = x0.xy + C.xx - i1;
  vec2 x2 = x0.xy + C.zz;

  i = mod289(i);
  vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));

  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);

  m = m*m;
  m = m*m;

  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;

  m *= 1.79284291400159 - 0.85373472095314 * (a0*a0+h*h);

  vec3 g = vec3(0.0);
  g.x  = a0.x * x0.x + h.x * x0.y;
  g.yz = a0.yz * vec2(x1.x,x2.x) + h.yz * vec2(x1.y,x2.y);
  return 130.0 * dot(m, g);
}

// implementation of MurmurHash (https://sites.google.com/site/murmurhash/) for a 
// single unsigned integer.
uint hash(uint x, uint seed) 
{
  const uint m = 0x5bd1e995U;
  uint hash = seed;
  // process input
  uint k = x;
  k *= m;
  k ^= k >> 24;
  k *= m;
  hash *= m;
  hash ^= k;
  // some final mixing
  hash ^= hash >> 13;
  hash *= m;
  hash ^= hash >> 15;
  return hash;
}

// implementation of MurmurHash (https://sites.google.com/site/murmurhash/) for a  
// 3-dimensional unsigned integer input vector.
uint hash(uvec3 x, uint seed)
{
  const uint m = 0x5bd1e995U;
  uint hash = seed;
  // process first vector element
  uint k = x.x; 
  k *= m;
  k ^= k >> 24;
  k *= m;
  hash *= m;
  hash ^= k;
  // process second vector element
  k = x.y; 
  k *= m;
  k ^= k >> 24;
  k *= m;
  hash *= m;
  hash ^= k;
  // process third vector element
  k = x.z; 
  k *= m;
  k ^= k >> 24;
  k *= m;
  hash *= m;
  hash ^= k;
  // some final mixing
  hash ^= hash >> 13;
  hash *= m;
  hash ^= hash >> 15;
  return hash;
}


// Thx to TheTurk for his Fast Improved 3D Perlin Noise
vec3 gradientDirection(uint hash) 
{
  switch (int(hash) & 15) // look at the last four bits to pick a gradient direction
  {
  case 0:
      return vec3(1, 1, 0);
  case 1:
      return vec3(-1, 1, 0);
  case 2:
      return vec3(1, -1, 0);
  case 3:
      return vec3(-1, -1, 0);
  case 4:
      return vec3(1, 0, 1);
  case 5:
      return vec3(-1, 0, 1);
  case 6:
      return vec3(1, 0, -1);
  case 7:
      return vec3(-1, 0, -1);
  case 8:
      return vec3(0, 1, 1);
  case 9:
      return vec3(0, -1, 1);
  case 10:
      return vec3(0, 1, -1);
  case 11:
      return vec3(0, -1, -1);
  case 12:
      return vec3(1, 1, 0);
  case 13:
      return vec3(-1, 1, 0);
  case 14:
      return vec3(0, -1, 1);
  case 15:
      return vec3(0, -1, -1);
  }
}

float interpolate(float value1, float value2, float value3, float value4, 
                  float value5, float value6, float value7, float value8, 
                  vec3 t)
{
  return mix(
    mix(mix(value1, value2, t.x), mix(value3, value4, t.x), t.y),
    mix(mix(value5, value6, t.x), mix(value7, value8, t.x), t.y),
    t.z);
}

vec3 fade(vec3 t)
{
  // 6t^5 - 15t^4 + 10t^3
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float perlinNoise(vec3 position, uint seed) 
{
  vec3 floorPosition = floor(position);
  vec3 fractPosition = position - floorPosition;
  uvec3 cellCoordinates = uvec3(floorPosition);
  float value1 = dot(gradientDirection(hash(cellCoordinates, seed)), fractPosition);
  float value2 = dot(gradientDirection(hash((cellCoordinates + uvec3(1, 0, 0)), seed)), fractPosition - vec3(1, 0, 0));
  float value3 = dot(gradientDirection(hash((cellCoordinates + uvec3(0, 1, 0)), seed)), fractPosition - vec3(0, 1, 0));
  float value4 = dot(gradientDirection(hash((cellCoordinates + uvec3(1, 1, 0)), seed)), fractPosition - vec3(1, 1, 0));
  float value5 = dot(gradientDirection(hash((cellCoordinates + uvec3(0, 0, 1)), seed)), fractPosition - vec3(0, 0, 1));
  float value6 = dot(gradientDirection(hash((cellCoordinates + uvec3(1, 0, 1)), seed)), fractPosition - vec3(1, 0, 1));
  float value7 = dot(gradientDirection(hash((cellCoordinates + uvec3(0, 1, 1)), seed)), fractPosition - vec3(0, 1, 1));
  float value8 = dot(gradientDirection(hash((cellCoordinates + uvec3(1, 1, 1)), seed)), fractPosition - vec3(1, 1, 1));
  return interpolate(value1, value2, value3, value4, value5, value6, value7, value8, fade(fractPosition));
}

float perlinNoise(vec3 position, int frequency, int octaveCount, float persistence, float lacunarity, uint seed) 
{
  float value = 0.0;
  float amplitude = 1.0;
  float currentFrequency = float(frequency);
  uint currentSeed = seed;
  for (int i = 0; i < octaveCount; i++) 
  {
    currentSeed = hash(currentSeed, 0x0U); // create a new seed for each octave
    value += perlinNoise(position * currentFrequency, currentSeed) * amplitude;
    amplitude *= persistence;
    currentFrequency *= lacunarity;
  }
  return value;
}

#endif
