#version 310 es

shared int v[3];
void tint_symbol(uint local_invocation_index) {
  {
    for(uint idx = local_invocation_index; (idx < 3u); idx = (idx + 1u)) {
      uint i = idx;
      v[i] = 0;
    }
  }
  barrier();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol(gl_LocalInvocationIndex);
  return;
}
