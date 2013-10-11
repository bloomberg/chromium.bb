/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Check whether the Mips atomic 64-bit operations are defined.
 *
 * These checks are encoded as "CHECK: regex", and the regexes must be
 * matched in-order in the disassembled .nexe file.  An external tool
 * "file-check" will do the checks provided this source file and the .nexe.
 */

#include <stdint.h>

/*
 * The use of 'volatile' is intended to pull in the atomic functions from
 * compiler-rt.
 */
volatile uint64_t num = 0x17;

int main(void) {
/* CHECK: <__sync_fetch_and_add_8>: */
/* CHECK: <__sync_fetch_and_sub_8>: */
/* CHECK: <__sync_fetch_and_and_8>: */
/* CHECK: <__sync_fetch_and_or_8>: */
/* CHECK: <__sync_fetch_and_xor_8>: */

/* CHECK: <__sync_add_and_fetch_8>: */
/* CHECK: <__sync_sub_and_fetch_8>: */
/* CHECK: <__sync_and_and_fetch_8>: */
/* CHECK: <__sync_or_and_fetch_8>: */
/* CHECK: <__sync_xor_and_fetch_8>: */

/* CHECK: <__sync_bool_compare_and_swap_8>: */
/* CHECK: <__sync_val_compare_and_swap_8>: */
/* CHECK: <__sync_lock_test_and_set_8>: */
/* CHECK: <__sync_lock_release_8>: */

  return (int) num;
}
