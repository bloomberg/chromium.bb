groupshared uint arg_0;

void atomicSub_0d26c2() {
  uint arg_1 = 1u;
  uint atomic_result = 0u;
  InterlockedAdd(arg_0, -arg_1, atomic_result);
  uint res = atomic_result;
}

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

void compute_main_inner(uint local_invocation_index) {
  {
    uint atomic_result_1 = 0u;
    InterlockedExchange(arg_0, 0u, atomic_result_1);
  }
  GroupMemoryBarrierWithGroupSync();
  atomicSub_0d26c2();
}

[numthreads(1, 1, 1)]
void compute_main(tint_symbol_1 tint_symbol) {
  compute_main_inner(tint_symbol.local_invocation_index);
  return;
}
