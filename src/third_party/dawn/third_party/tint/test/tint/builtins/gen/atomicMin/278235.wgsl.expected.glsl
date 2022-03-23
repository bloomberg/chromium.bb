#version 310 es

shared int arg_0;
void atomicMin_278235() {
  int res = atomicMin(arg_0, 1);
}

void compute_main(uint local_invocation_index) {
  {
    atomicExchange(arg_0, 0);
  }
  barrier();
  atomicMin_278235();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main(gl_LocalInvocationIndex);
  return;
}
