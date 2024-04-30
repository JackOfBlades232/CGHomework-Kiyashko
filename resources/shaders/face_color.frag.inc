const vec4 x_min_col = vec4(0.1, 0.8, 0.12, 1.0);
const vec4 x_max_col = vec4(0.8, 0.12, 0.1, 1.0);
const vec4 y_min_col = vec4(0.8, 0.8, 0.8, 1.0);
const vec4 y_max_col = vec4(0.1, 0.1, 0.1, 1.0);
const vec4 z_min_col = vec4(0.4, 0.6, 0.8, 1.0);
const vec4 z_max_col = vec4(0.8, 0.7, 0.4, 1.0);

vec4 color_from_normal(vec3 norm)
{
  vec3 ratios = (norm + vec3(1.f))*0.5f;
  return mix(x_min_col, x_max_col, ratios.x) + 
         mix(y_min_col, y_max_col, ratios.y) +
         mix(z_min_col, z_max_col, ratios.z);
}
