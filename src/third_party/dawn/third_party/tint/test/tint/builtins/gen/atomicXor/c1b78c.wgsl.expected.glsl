#version 310 es
precision mediump float;

struct SB_RW {
  int arg_0;
};

layout(binding = 0, std430) buffer SB_RW_1 {
  int arg_0;
} sb_rw;
void atomicXor_c1b78c() {
  int res = atomicXor(sb_rw.arg_0, 1);
}

void fragment_main() {
  atomicXor_c1b78c();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

struct SB_RW {
  int arg_0;
};

layout(binding = 0, std430) buffer SB_RW_1 {
  int arg_0;
} sb_rw;
void atomicXor_c1b78c() {
  int res = atomicXor(sb_rw.arg_0, 1);
}

void compute_main() {
  atomicXor_c1b78c();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
