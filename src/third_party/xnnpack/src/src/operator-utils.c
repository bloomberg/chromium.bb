// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <xnnpack.h>           // For xnn_caches_t, xnn_operator_t.
#include <xnnpack/allocator.h> // For XNN_ALLOCATION_ALIGNMENT.
#include <xnnpack/cache.h>     // For xnn_caches.
#include <xnnpack/operator.h>  // For xnn_operator definition.

void* xnn_get_pointer_to_write_weights(
  xnn_operator_t op,
  xnn_caches_t caches,
  size_t aligned_weights_size,
  int padding_byte)
{
  assert(aligned_weights_size % XNN_ALLOCATION_ALIGNMENT == 0);
  void* weights_ptr = NULL;
  if (use_weights_cache(caches)) {
    weights_ptr = xnn_reserve_space_in_weights_cache(caches->weights_cache, aligned_weights_size);
    if (weights_ptr == NULL) {
      return NULL;
    }
  } else {
    op->packed_weights.pointer = xnn_allocate_simd_memory(aligned_weights_size);
    if (op->packed_weights.pointer == NULL) {
      return NULL;
    }
    weights_ptr = op->packed_weights.pointer;
  }
  memset(weights_ptr, padding_byte, aligned_weights_size);
  return weights_ptr;
}

// Calculate how much work a microkernel does.
// A MxN microkernel does M+N (scalar) loads and M*N (scalar) FMAs.
// So, given batch_size, the microkernel does:
//   divide_round_up(batch_size, mr) * (mr + nr) loads, and
//   divide_round_up(batch_size, mr) * (mr * nr) FMAs.
// The total cost is then a linear combination of these 2 operations. From experimental data, use a multiplier of 3 for
// loads, to prefer higher tile sizes which have better computation intensity.
static size_t calculate_microkernel_cost(size_t batch_size, uint32_t mr, uint32_t nr)
{
  return divide_round_up(batch_size, mr) * (3 * (mr + nr) + mr * nr);
}

uint32_t xnn_get_heuristic_mr_gemm(
  size_t batch_size, uint32_t max_mr, uint32_t nr, struct xnn_hmp_gemm_ukernel *gemm_cases)
{
  assert(gemm_cases[max_mr-1].function[XNN_UARCH_DEFAULT] != NULL);
  if (batch_size <= max_mr && gemm_cases[batch_size-1].function[XNN_UARCH_DEFAULT] != NULL) {
    // We have a microkernel with MR that is the exact match with batch_size.
    return batch_size;
  }

  // Try to find the best fitting mr.
  // - use a cost heuristic to calculate how much work is done by the microkernel (see calculate_microkernel_cost)
  // - smaller cost is better
  uint32_t best_mr = max_mr;
  size_t best_cost = SIZE_MAX;
  for (uint32_t mr = 1; mr <= max_mr; mr++) {
    if (gemm_cases[mr-1].function[XNN_UARCH_DEFAULT] == NULL) {
      continue;
    }
    const size_t current_cost = calculate_microkernel_cost(batch_size, mr, nr);
    if (current_cost <= best_cost) {
      best_mr = mr;
      best_cost = current_cost;
    }
  }
  assert(gemm_cases[best_mr-1].function[XNN_UARCH_DEFAULT] != NULL);
  return best_mr;
}

uint32_t xnn_get_heuristic_mr_igemm(
  size_t batch_size, uint32_t max_mr, uint32_t nr, struct xnn_hmp_igemm_ukernel *igemm_cases)
{
  assert(igemm_cases[max_mr-1].function[XNN_UARCH_DEFAULT] != NULL);
  if (batch_size <= max_mr && igemm_cases[batch_size-1].function[XNN_UARCH_DEFAULT] != NULL) {
    // We have a microkernel with MR that is the exact match with batch_size.
    return batch_size;
  }

  // Try to find the best fitting mr.
  // - use a cost heuristic to calculate how much work is done by the microkernel (see calculate_microkernel_cost)
  // - smaller cost is better
  uint32_t best_mr = max_mr;
  size_t best_cost = SIZE_MAX;
  for (uint32_t mr = 1; mr <= max_mr; mr++) {
    if (igemm_cases[mr-1].function[XNN_UARCH_DEFAULT] == NULL) {
      continue;
    }
    const size_t current_cost = calculate_microkernel_cost(batch_size, mr, nr);
    if (current_cost <= best_cost) {
      best_mr = mr;
      best_cost = current_cost;
    }
  }
  assert(igemm_cases[best_mr-1].function[XNN_UARCH_DEFAULT] != NULL);
  return best_mr;
}
