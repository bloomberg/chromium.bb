#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
int add_int_min_explicit() {
  int a = -2147483648;
  int b = (a + 1);
  int c = (-2147483648 + 1);
  return c;
}

