struct atomic_compare_exchange_resultu32 {
  uint old_value;
  bool exchanged;
};
struct atomic_compare_exchange_resulti32 {
  int old_value;
  bool exchanged;
};
RWByteAddressBuffer a_u32 : register(u0, space0);
RWByteAddressBuffer a_i32 : register(u1, space0);
groupshared uint b_u32;
groupshared int b_i32;

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};
struct atomic_compare_exchange_weak_ret_type {
  uint old_value;
  bool exchanged;
};

atomic_compare_exchange_weak_ret_type tint_atomicCompareExchangeWeak(RWByteAddressBuffer buffer, uint offset, uint compare, uint value) {
  atomic_compare_exchange_weak_ret_type result=(atomic_compare_exchange_weak_ret_type)0;
  buffer.InterlockedCompareExchange(offset, compare, value, result.old_value);
  result.exchanged = result.old_value == compare;
  return result;
}


struct atomic_compare_exchange_weak_ret_type_1 {
  int old_value;
  bool exchanged;
};

atomic_compare_exchange_weak_ret_type_1 tint_atomicCompareExchangeWeak_1(RWByteAddressBuffer buffer, uint offset, int compare, int value) {
  atomic_compare_exchange_weak_ret_type_1 result=(atomic_compare_exchange_weak_ret_type_1)0;
  buffer.InterlockedCompareExchange(offset, compare, value, result.old_value);
  result.exchanged = result.old_value == compare;
  return result;
}


void main_inner(uint local_invocation_index) {
  if ((local_invocation_index < 1u)) {
    uint atomic_result = 0u;
    InterlockedExchange(b_u32, 0u, atomic_result);
    int atomic_result_1 = 0;
    InterlockedExchange(b_i32, 0, atomic_result_1);
  }
  GroupMemoryBarrierWithGroupSync();
  {
    uint value = 42u;
    const atomic_compare_exchange_weak_ret_type r1 = tint_atomicCompareExchangeWeak(a_u32, 0u, 0u, value);
    const atomic_compare_exchange_weak_ret_type r2 = tint_atomicCompareExchangeWeak(a_u32, 0u, 0u, value);
    const atomic_compare_exchange_weak_ret_type r3 = tint_atomicCompareExchangeWeak(a_u32, 0u, 0u, value);
  }
  {
    int value = 42;
    const atomic_compare_exchange_weak_ret_type_1 r1 = tint_atomicCompareExchangeWeak_1(a_i32, 0u, 0, value);
    const atomic_compare_exchange_weak_ret_type_1 r2 = tint_atomicCompareExchangeWeak_1(a_i32, 0u, 0, value);
    const atomic_compare_exchange_weak_ret_type_1 r3 = tint_atomicCompareExchangeWeak_1(a_i32, 0u, 0, value);
  }
  {
    uint value = 42u;
    atomic_compare_exchange_resultu32 atomic_result_2 = (atomic_compare_exchange_resultu32)0;
    uint atomic_compare_value = 0u;
    InterlockedCompareExchange(b_u32, atomic_compare_value, value, atomic_result_2.old_value);
    atomic_result_2.exchanged = atomic_result_2.old_value == atomic_compare_value;
    const atomic_compare_exchange_resultu32 r1 = atomic_result_2;
    atomic_compare_exchange_resultu32 atomic_result_3 = (atomic_compare_exchange_resultu32)0;
    uint atomic_compare_value_1 = 0u;
    InterlockedCompareExchange(b_u32, atomic_compare_value_1, value, atomic_result_3.old_value);
    atomic_result_3.exchanged = atomic_result_3.old_value == atomic_compare_value_1;
    const atomic_compare_exchange_resultu32 r2 = atomic_result_3;
    atomic_compare_exchange_resultu32 atomic_result_4 = (atomic_compare_exchange_resultu32)0;
    uint atomic_compare_value_2 = 0u;
    InterlockedCompareExchange(b_u32, atomic_compare_value_2, value, atomic_result_4.old_value);
    atomic_result_4.exchanged = atomic_result_4.old_value == atomic_compare_value_2;
    const atomic_compare_exchange_resultu32 r3 = atomic_result_4;
  }
  {
    int value = 42;
    atomic_compare_exchange_resulti32 atomic_result_5 = (atomic_compare_exchange_resulti32)0;
    int atomic_compare_value_3 = 0;
    InterlockedCompareExchange(b_i32, atomic_compare_value_3, value, atomic_result_5.old_value);
    atomic_result_5.exchanged = atomic_result_5.old_value == atomic_compare_value_3;
    const atomic_compare_exchange_resulti32 r1 = atomic_result_5;
    atomic_compare_exchange_resulti32 atomic_result_6 = (atomic_compare_exchange_resulti32)0;
    int atomic_compare_value_4 = 0;
    InterlockedCompareExchange(b_i32, atomic_compare_value_4, value, atomic_result_6.old_value);
    atomic_result_6.exchanged = atomic_result_6.old_value == atomic_compare_value_4;
    const atomic_compare_exchange_resulti32 r2 = atomic_result_6;
    atomic_compare_exchange_resulti32 atomic_result_7 = (atomic_compare_exchange_resulti32)0;
    int atomic_compare_value_5 = 0;
    InterlockedCompareExchange(b_i32, atomic_compare_value_5, value, atomic_result_7.old_value);
    atomic_result_7.exchanged = atomic_result_7.old_value == atomic_compare_value_5;
    const atomic_compare_exchange_resulti32 r3 = atomic_result_7;
  }
}

[numthreads(16, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.local_invocation_index);
  return;
}
