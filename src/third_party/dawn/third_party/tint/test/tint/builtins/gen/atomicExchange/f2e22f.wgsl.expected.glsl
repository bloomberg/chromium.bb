#version 310 es
precision mediump float;

struct SB_RW {
  int arg_0;
};

layout(binding = 0, std430) buffer SB_RW_1 {
  int arg_0;
} sb_rw;
void atomicExchange_f2e22f() {
  int res = atomicExchange(sb_rw.arg_0, 1);
}

void fragment_main() {
  atomicExchange_f2e22f();
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
void atomicExchange_f2e22f() {
  int res = atomicExchange(sb_rw.arg_0, 1);
}

void compute_main() {
  atomicExchange_f2e22f();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
