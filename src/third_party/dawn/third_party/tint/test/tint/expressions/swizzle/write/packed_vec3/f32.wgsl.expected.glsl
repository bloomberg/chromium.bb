#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct S {
  vec3 v;
};

layout(binding = 0, std430) buffer S_1 {
  vec3 v;
} U;
void f() {
  U.v = vec3(1.0f, 2.0f, 3.0f);
  U.v.x = 1.0f;
  U.v.y = 2.0f;
  U.v.z = 3.0f;
}

