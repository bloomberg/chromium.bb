/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * These are simple unit tests which exercise the conversion between trusted
 * and untrusted pointers in MinSFI.
 */

/*
 * Tell the minsfi_ptr.h header to generate code which returns a magic value
 * if an error is detected, rather than abort.
 */
#define MINSFI_PTR_CONVERSION_TEST

#include "native_client/src/include/minsfi_ptr.h"
#include "native_client/src/include/nacl_assert.h"

static MinsfiSandbox sb;
static sfiptr_t untrusted_ptr;
static char *native_ptr;

void test_from_ptr_masks_pointer(void) {
  sb.mem_base   = (char*)    0x00000000;
  sb.ptr_mask   = (sfiptr_t) 0x0000FFFF;

  untrusted_ptr = (sfiptr_t) 0xABCDEF12;
  native_ptr    = (char*)    0x0000EF12;

  ASSERT_EQ(native_ptr, FromMinsfiPtr(untrusted_ptr, &sb));
}

void test_from_ptr_adds_base(void) {
  sb.mem_base   = (char*)    0xABCD1000;
  sb.ptr_mask   = (sfiptr_t) 0xFFFFFFFF;

  untrusted_ptr = (sfiptr_t) 0x00123456;
  native_ptr    = (char*)    0xABDF4456;

  ASSERT_EQ(native_ptr, FromMinsfiPtr(untrusted_ptr, &sb));
}

void test_to_ptr_subtracts_base(void) {
  sb.mem_base   = (char*)    0x11111000;
  sb.ptr_mask   = (sfiptr_t) 0xFFFFFFFF;

  native_ptr    = (char*)    0x11234567;
  untrusted_ptr = (sfiptr_t) 0x00123567;

  ASSERT_EQ(untrusted_ptr, ToMinsfiPtr(native_ptr, &sb));
}

void test_to_ptr_checks_lower_bound(void) {
  sb.mem_base   = (char*)    0x11111000;
  sb.ptr_mask   = (sfiptr_t) 0xFFFFFFFF;

  /* last rejected pointer */
  native_ptr    = (char*)    0x11110FFF;
  untrusted_ptr = (sfiptr_t) 0xCAFEBABE;  /* mock error code */

  ASSERT_EQ(untrusted_ptr, ToMinsfiPtr(native_ptr, &sb));

  /* first accepted pointer */
  native_ptr    = (char*)    0x11111000;
  untrusted_ptr = (sfiptr_t) 0x00000000;

  ASSERT_EQ(untrusted_ptr, ToMinsfiPtr(native_ptr, &sb));
}

void test_to_ptr_checks_upper_bound(void) {
  sb.mem_base   = (char*)    0x11110000;
  sb.ptr_mask   = (sfiptr_t) 0x0000FFFF;

  /* last accepted pointer */
  native_ptr    = (char*)    0x1111FFFF;
  untrusted_ptr = (sfiptr_t) 0x0000FFFF;

  ASSERT_EQ(untrusted_ptr, ToMinsfiPtr(native_ptr, &sb));

  /* first rejected pointer */
  native_ptr    = (char*)    0x11120000;
  untrusted_ptr = (sfiptr_t) 0xCAFEBABE;  /* mock error code */

  ASSERT_EQ(untrusted_ptr, ToMinsfiPtr(native_ptr, &sb));
}

int main(void) {
  test_from_ptr_masks_pointer();
  test_from_ptr_adds_base();
  test_to_ptr_subtracts_base();
  test_to_ptr_checks_lower_bound();
  test_to_ptr_checks_upper_bound();
  return 0;
}
