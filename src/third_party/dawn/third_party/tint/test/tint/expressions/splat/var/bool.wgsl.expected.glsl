#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
void f() {
  bool tint_tmp = true;
  if (!tint_tmp) {
    tint_tmp = false;
  }
  bool v = (tint_tmp);
  bvec2 v2 = bvec2(v);
  bvec3 v3 = bvec3(v);
  bvec4 v4 = bvec4(v);
}

