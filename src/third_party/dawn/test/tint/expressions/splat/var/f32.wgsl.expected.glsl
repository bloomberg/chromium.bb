#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
void f() {
  float v = (1.0f + 2.0f);
  vec2 v2 = vec2(v);
  vec3 v3 = vec3(v);
  vec4 v4 = vec4(v);
}

