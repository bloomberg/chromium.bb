/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This tests the generation of memory layout for a MinSFI sandbox.
 * First, we check that the generated limits match the expected values,
 * and then we verify that the algorithm fails on invalid inputs.
 */

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"
#include "native_client/src/include/nacl_assert.h"

static const unsigned page_size = 0x1000;
MinsfiMemoryLayout mem;

MinsfiManifest InitManifest(void) {
  MinsfiManifest sb;
  sb.ptr_size = 24;
  sb.dataseg_offset = 0x1000;
  sb.dataseg_size = 0x2400;  /* intentionally not page-aligned and larger than
                                page_size to test rounding up */

  ASSERT_EQ(true, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));
  return sb;
}

void test_valid_layout(void) {
  MinsfiManifest sb = InitManifest();

  ASSERT_EQ(true, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));

  ASSERT_EQ(   0x1000, mem.dataseg.offset);
  ASSERT_EQ(   0x4000, mem.dataseg.offset + mem.dataseg.length);
  ASSERT_EQ(   0x5000, mem.heap.offset);
  ASSERT_EQ( 0xFDF000, mem.heap.offset + mem.heap.length);
  ASSERT_EQ( 0xFE0000, mem.stack.offset);
  ASSERT_EQ(0x1000000, mem.stack.offset + mem.stack.length);
}

void test_page_size_not_pow2(void) {
  MinsfiManifest sb = InitManifest();
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, 1234, &mem));
}

void test_ptrsize_invalid(void) {
  MinsfiManifest sb = InitManifest();

  sb.ptr_size = 0;
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));

  sb.ptr_size = 19;
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));

  sb.ptr_size = 32;
  ASSERT_EQ(true, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));

  sb.ptr_size = 33;
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));
}

void test_dataseg_offset_invalid(void) {
  MinsfiManifest sb = InitManifest();

  /* offset not page-aligned */
  sb.dataseg_offset = page_size + 7;
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));

  /* offset out of bounds */
  sb.dataseg_offset = (1 << (sb.ptr_size + 1));
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));
}

void test_dataseg_size_invalid(void) {
  MinsfiManifest sb = InitManifest();

  /* end of the region out of bounds; size gets rounded up and fails */
  sb.dataseg_size = (1 << sb.ptr_size) - (page_size + 1);
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));
}

void test_no_space_for_heap(void) {
  MinsfiManifest sb = InitManifest();
  bool ret;

  /* heap size exactly one page */
  sb.dataseg_size = (1 << sb.ptr_size) - sb.dataseg_offset - 35 * page_size;
  ret = MinsfiGenerateMemoryLayout(&sb, page_size, &mem);
  ASSERT_EQ(true, ret);
  ASSERT_EQ(page_size, mem.heap.length);

  /* heap size less than one page. */
  sb.dataseg_size += 1;
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));

  /* heap size negative */
  sb.dataseg_size += page_size;
  ASSERT_EQ(false, MinsfiGenerateMemoryLayout(&sb, page_size, &mem));
}

int main(void) {
  test_valid_layout();
  test_page_size_not_pow2();
  test_ptrsize_invalid();
  test_dataseg_offset_invalid();
  test_dataseg_size_invalid();
  test_no_space_for_heap();
  return 0;
}
