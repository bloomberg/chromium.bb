/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#if defined(__GLIBC__)
# include <link.h>
#endif
#include <nacl/nacl_list_mappings.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_list_mappings.h"

#if defined(__GLIBC__)
static const uint32_t kDynamicTextEnd = 1 << 28;
#endif

static void test_list_mappings_read_write(void) {
  void *blk;

  printf("Testing list_mappings on a read-write area.\n");

  blk = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  ASSERT_NE(blk, MAP_FAILED);

  struct NaClMemMappingInfo map[0x10000];
  size_t capacity = sizeof(map) / sizeof(*map);
  size_t size;
  int result = nacl_list_mappings(map, capacity, &size);
  ASSERT_EQ(0, result);
  ASSERT_LE(size, capacity);

  bool found = false;
  for (size_t i = 0; i < size; ++i) {
    if (map[i].start == (uint32_t) blk && map[i].size == 0x10000 &&
        map[i].prot == (PROT_READ | PROT_WRITE)) {
      found = true;
      break;
    }
  }
  ASSERT(found);
}

static void test_list_mappings_bad_destination(void) {
  printf("Testing list_mappings on a bad destination.\n");

  size_t got;
  int result = nacl_list_mappings(
      reinterpret_cast<struct NaClMemMappingInfo *>(0x0),
      1024 * 1024, &got);
  ASSERT_NE(0, result);
  ASSERT_EQ(EFAULT, errno);
}

#if defined(__GLIBC__)

struct TestListMappingsPhdrs {
  struct NaClMemMappingInfo map[0x10000];
  size_t size;
};

static int visit_phdrs(struct dl_phdr_info *info, size_t size, void *data) {
  struct TestListMappingsPhdrs *state = (struct TestListMappingsPhdrs *) data;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    if (info->dlpi_phdr[i].p_type != PT_LOAD)
      continue;
    uint32_t start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
    uint32_t memsz = info->dlpi_phdr[i].p_memsz;
    uint32_t flags = info->dlpi_phdr[i].p_flags;
    // Some regions appear in phdr as PROT_EXEC when they cannot be.
    if (start >= kDynamicTextEnd) {
      flags &= (PROT_READ | PROT_WRITE);
    }
    bool found = false;
    for (size_t j = 0; j < state->size; ++j) {
      uint32_t mstart = state->map[j].start;
      if (start >= mstart &&
          start + memsz <= mstart + state->map[j].size &&
          (flags & state->map[j].prot) == flags) {
        found = true;
        break;
      }
    }
    ASSERT(found);
  }
  return 0;
}

static void test_list_mappings_phdrs(void) {
  struct TestListMappingsPhdrs state;

  printf("Testing list_mappings on all the phdrs.\n");

  size_t capacity = sizeof(state.map) / sizeof(*state.map);
  int result = nacl_list_mappings(state.map, capacity, &state.size);
  ASSERT_EQ(0, result);
  ASSERT_LE(state.size, capacity);

  dl_iterate_phdr(visit_phdrs, &state);
}

#endif

static void test_list_mappings_order_and_overlap(void) {
  printf("Testing list_mappings order and lack of overlap.\n");

  struct NaClMemMappingInfo map[0x10000];
  size_t capacity = sizeof(map) / sizeof(*map);
  size_t size;
  int result = nacl_list_mappings(map, capacity, &size);
  ASSERT_EQ(0, result);
  ASSERT_LE(size, capacity);

  for (uint32_t i = 0; i < size - 1; ++i) {
    ASSERT_LE(map[i].start + map[i].size, map[i + 1].start);
  }
}

int main(void) {
  test_list_mappings_read_write();
  test_list_mappings_bad_destination();
#if defined(__GLIBC__)
  test_list_mappings_phdrs();
#endif
  test_list_mappings_order_and_overlap();
  return 0;
}
