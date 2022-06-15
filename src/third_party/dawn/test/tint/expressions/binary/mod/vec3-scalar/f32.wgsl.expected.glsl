#version 310 es

vec3 tint_float_modulo(vec3 lhs, float rhs) {
  return (lhs - rhs * trunc(lhs / rhs));
}


void f() {
  vec3 a = vec3(1.0f, 2.0f, 3.0f);
  vec3 r = tint_float_modulo(vec3(1.0f, 2.0f, 3.0f), 4.0f);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
