#version 310 es

float tint_float_modulo(float lhs, float rhs) {
  return (lhs - rhs * trunc(lhs / rhs));
}


layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
int a = 0;
float b = 0.0f;
void foo(int maybe_zero) {
  a = (a / 0);
  a = (a % 0);
  a = (a / maybe_zero);
  a = (a % maybe_zero);
  b = (b / 0.0f);
  b = tint_float_modulo(b, 0.0f);
  b = (b / float(maybe_zero));
  b = tint_float_modulo(b, float(maybe_zero));
}

