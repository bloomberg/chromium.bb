/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <sys/nacl_syscalls.h>

#include "native_client/tests/dynamic_code_loading/dynamic_segment.h"
#include "native_client/tests/dynamic_code_loading/templates.h"
#include "native_client/tests/inbrowser_test_runner/test_runner.h"

#if defined(__x86_64__)
/* On x86-64, template functions do not fit in 32-byte buffers */
#define BUF_SIZE 64
#elif defined(__i386__) || defined(__arm__)
#define BUF_SIZE 32
#else
#error "Unknown Platform"
#endif

#if defined(__i386__) || defined(__x86_64__)
#define NACL_BUNDLE_SIZE  32
#elif defined(__arm__)
#define NACL_BUNDLE_SIZE  16
#else
#error "Unknown Platform"
#endif

/*
 * TODO(bsy): get this value from the toolchain.  Get the toolchain
 * team to provide this value.
 */
#define NUM_BUNDLES_FOR_HLT 3

int nacl_load_code(void *dest, void *src, int size) {
  int rc = nacl_dyncode_create(dest, src, size);
  /* Undo the syscall wrapper's errno handling, because it's more
     convenient to test a single return value. */
  return rc == 0 ? 0 : -errno;
}

char *next_addr = NULL;

char *allocate_code_space(int pages) {
  char *addr;
  if (next_addr == NULL) {
    next_addr = (char *) DYNAMIC_CODE_SEGMENT_START;
  }
  addr = next_addr;
  next_addr += 0x10000 * pages;
  assert(next_addr < (char *) DYNAMIC_CODE_SEGMENT_END);
  return addr;
}

void fill_int32(uint8_t *data, size_t size, int32_t value) {
  int i;
  assert(size % 4 == 0);
  /* All the archs we target supported unaligned word read/write, but
     check that the pointer is aligned anyway. */
  assert(((uintptr_t) data) % 4 == 0);
  for (i = 0; i < size / 4; i++)
    ((uint32_t *) data)[i] = value;
}

void fill_nops(uint8_t *data, size_t size) {
#if defined(__i386__) || defined(__x86_64__)
  memset(data, 0x90, size); /* NOPs */
#elif defined(__arm__)
  fill_int32(data, size, 0xe1a00000); /* NOP (MOV r0, r0) */
#else
# error "Unknown arch"
#endif
}

void fill_hlts(uint8_t *data, size_t size) {
#if defined(__i386__) || defined(__x86_64__)
  memset(data, 0xf4, size); /* HLTs */
#elif defined(__arm__)
  fill_int32(data, size, 0xe1266676); /* BKPT 0x6666 */
#else
# error "Unknown arch"
#endif
}

/* Getting the assembler to pad our code fragments in templates.S is
   awkward because we have to output them in data mode, in which the
   assembler wants to output zeroes instead of NOPs for padding.
   Also, the assembler won't put in a terminating HLT, which we need
   on x86-32.  So we do the padding at run time. */
void copy_and_pad_fragment(void *dest,
                           int dest_size,
                           const char *fragment_start,
                           const char *fragment_end) {
  int fragment_size = fragment_end - fragment_start;
  assert(dest_size % NACL_BUNDLE_SIZE == 0);
  assert(fragment_size <= dest_size);
  fill_nops(dest, dest_size);
  memcpy(dest, fragment_start, fragment_size);
}

/* Check that we can load and run code. */
void test_loading_code() {
  void *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);

  rc = nacl_load_code(load_area, buf, sizeof(buf));
  assert(rc == 0);
  assert(memcmp(load_area, buf, sizeof(buf)) == 0);
  /* Need double cast otherwise gcc complains with "ISO C forbids
     conversion of object pointer to function pointer type
     [-pedantic]". */
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);
}

/* The syscall may have to mmap() shared memory temporarily,
   so there is some interaction with page size.
   Check that we can load to non-page-aligned addresses. */
void test_loading_code_non_page_aligned() {
  char *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);

  rc = nacl_load_code(load_area, buf, sizeof(buf));
  assert(rc == 0);
  assert(memcmp(load_area, buf, sizeof(buf)) == 0);

  load_area += sizeof(buf);
  rc = nacl_load_code(load_area, buf, sizeof(buf));
  assert(rc == 0);
  assert(memcmp(load_area, buf, sizeof(buf)) == 0);
}

/* Since there is an interaction with page size, we also test loading
   a multi-page chunk of code. */
void test_loading_large_chunk() {
  char *load_area = allocate_code_space(2);
  int size = 0x20000;
  uint8_t *data = alloca(size);
  int rc;

  fill_nops(data, size);
  rc = nacl_load_code(load_area, data, size);
  assert(rc == 0);
  assert(memcmp(load_area, data, size) == 0);
}

void test_loading_zero_size() {
  char *load_area = allocate_code_space(1);
  int rc = nacl_load_code(load_area, &template_func, 0);
  assert(rc == 0);
}

/* In general, the failure tests don't check that loading fails for
   the reason we expect.  TODO(mseaborn): We could do this by
   comparing with expected log output. */

void test_fail_on_validation_error() {
  void *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;

  copy_and_pad_fragment(buf, sizeof(buf), &invalid_code, &invalid_code_end);

  rc = nacl_load_code(load_area, buf, sizeof(buf));
  assert(rc == -EINVAL);
}

void test_fail_on_non_bundle_aligned_dest_addresses() {
  char *load_area = allocate_code_space(1);
  int rc;
  uint8_t nops[BUF_SIZE];

  fill_nops(nops, sizeof(nops));

  /* Test unaligned destination. */
  rc = nacl_load_code(load_area + 1, nops, NACL_BUNDLE_SIZE);
  assert(rc == -EINVAL);
  rc = nacl_load_code(load_area + 4, nops, NACL_BUNDLE_SIZE);
  assert(rc == -EINVAL);

  /* Test unaligned size. */
  rc = nacl_load_code(load_area, nops + 1, NACL_BUNDLE_SIZE - 1);
  assert(rc == -EINVAL);
  rc = nacl_load_code(load_area, nops + 4, NACL_BUNDLE_SIZE - 4);
  assert(rc == -EINVAL);

  /* Check that the code we're trying works otherwise. */
  rc = nacl_load_code(load_area, nops, NACL_BUNDLE_SIZE);
  assert(rc == 0);
}

/* In principle we could load into the initially-loaded executable's
   code area, but at the moment we don't allow it. */
void test_fail_on_load_to_static_code_area() {
  int size = &hlts_end - &hlts;
  int rc = nacl_load_code(&hlts, &hlts, size);
  assert(rc == -EFAULT);
}

uint8_t block_in_data_segment[64];

void test_fail_on_load_to_data_area() {
  uint8_t *data;
  int rc;

  fill_hlts(block_in_data_segment, sizeof(block_in_data_segment));

  /* Align to bundle size so that we don't fail for a reason
     we're not testing for. */
  data = block_in_data_segment;
  while (((int) data) % NACL_BUNDLE_SIZE != 0)
    data++;
  rc = nacl_load_code(data, data, NACL_BUNDLE_SIZE);
  assert(rc == -EFAULT);
}

void test_fail_on_overwrite() {
  void *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);

  rc = nacl_load_code(load_area, buf, sizeof(buf));
  assert(rc == 0);

  copy_and_pad_fragment(buf, sizeof(buf), &template_func,
                                          &template_func_end);

  rc = nacl_load_code(load_area, buf, sizeof(buf));
  assert(rc == -EINVAL);
}


/* Allowing mmap() to overwrite the dynamic code area would be unsafe. */
void test_fail_on_mmap_to_dyncode_area() {
  void *addr = allocate_code_space(1);
  size_t page_size = 0x10000;
  void *result;
  int rc;

  assert((uintptr_t) addr % page_size == 0);
  result = mmap(addr, page_size, PROT_READ | PROT_WRITE,
                MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(result == MAP_FAILED);
  assert(errno == EINVAL);

  rc = munmap(addr, page_size);
  assert(rc == -1);
  assert(errno == EINVAL);

  /* TODO(mseaborn): Test mprotect() once NaCl provides it. */
}

void test_branches_outside_chunk() {
  char *load_area = allocate_code_space(1);
  int rc;
  int size = &branch_forwards_end - &branch_forwards;
  assert(size == 16 || size == 32);
  assert(&branch_backwards_end - &branch_backwards == size);

  rc = nacl_load_code(load_area, &branch_forwards, size);
  assert(rc == 0);
  rc = nacl_load_code(load_area + size, &branch_backwards, size);
  assert(rc == 0);
}

void test_end_of_code_region() {
  int rc;
  void *dest;
  uint8_t data[BUF_SIZE];
  fill_nops(data, sizeof(data));

  /* This tries to load into the data segment, which is definitely not
     allowed. */
  dest = (uint8_t *) DYNAMIC_CODE_SEGMENT_END;
  rc = nacl_load_code(dest, data, sizeof(data));
  assert(rc == -EFAULT);

  /* This tries to load into the last bundle of the code region, which
     sel_ldr disallows just in case there is some CPU bug in which the
     CPU fails to check for running off the end of an x86 code
     segment.  This is applied to other architectures for
     consistency. */
  dest = (uint8_t *) DYNAMIC_CODE_SEGMENT_END - sizeof(data);
  rc = nacl_load_code(dest, data, sizeof(data));
  assert(rc == -EFAULT);

  dest = (uint8_t *) DYNAMIC_CODE_SEGMENT_END - sizeof(data) * 2;
  rc = nacl_load_code(dest, data, sizeof(data));
  assert(rc == 0);
}

void test_hlt_filled_bundle() {
  uint8_t bad_code[NUM_BUNDLES_FOR_HLT * NACL_BUNDLE_SIZE];
  void *load_area;
  int ix;

  for (ix = 0; ix < NUM_BUNDLES_FOR_HLT; ++ix) {
    fill_nops(bad_code, sizeof bad_code);
    fill_hlts(bad_code + ix * NACL_BUNDLE_SIZE, NACL_BUNDLE_SIZE);

    load_area = allocate_code_space(1);
    /* hlts are now allowed */
    assert(0 == nacl_load_code(load_area, bad_code, sizeof bad_code));
    /* but not twice... */
    assert(0 != nacl_load_code(load_area, bad_code, sizeof bad_code));
  }
}


void run_test(const char *test_name, void (*test_func)(void)) {
  printf("Running %s...\n", test_name);
  test_func();
}

#define RUN_TEST(test_func) (run_test(#test_func, test_func))

int TestMain() {
  /*
   * This should come first, so that we test loading code into the first page.
   * See http://code.google.com/p/nativeclient/issues/detail?id=1143
   */
  RUN_TEST(test_loading_code);

  RUN_TEST(test_loading_code_non_page_aligned);
  RUN_TEST(test_loading_large_chunk);
  RUN_TEST(test_loading_zero_size);
  RUN_TEST(test_fail_on_validation_error);
  RUN_TEST(test_fail_on_non_bundle_aligned_dest_addresses);
  RUN_TEST(test_fail_on_load_to_static_code_area);
  RUN_TEST(test_fail_on_load_to_data_area);
  RUN_TEST(test_fail_on_overwrite);
  RUN_TEST(test_fail_on_mmap_to_dyncode_area);
  RUN_TEST(test_branches_outside_chunk);
  RUN_TEST(test_end_of_code_region);
  RUN_TEST(test_hlt_filled_bundle);

  /* Test again to make sure we didn't run out of space. */
  RUN_TEST(test_loading_code);

  return 0;
}

int main() {
  return RunTests(TestMain);
}
