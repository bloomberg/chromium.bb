#version 310 es
precision mediump float;

struct SB_RW {
  uint arg_0;
};

layout(binding = 0, std430) buffer SB_RW_1 {
  uint arg_0;
} sb_rw;
void atomicAnd_85a8d9() {
  uint arg_1 = 1u;
  uint res = atomicAnd(sb_rw.arg_0, arg_1);
}

void fragment_main() {
  atomicAnd_85a8d9();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

struct SB_RW {
  uint arg_0;
};

layout(binding = 0, std430) buffer SB_RW_1 {
  uint arg_0;
} sb_rw;
void atomicAnd_85a8d9() {
  uint arg_1 = 1u;
  uint res = atomicAnd(sb_rw.arg_0, arg_1);
}

void compute_main() {
  atomicAnd_85a8d9();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
