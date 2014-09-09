/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <sys/mman.h>

#include "native_client/src/include/minsfi_priv.h"

static inline bool IsPowerOfTwo(uint32_t x) {
  return ((x != 0) && !(x & (x - 1)));
}

static inline bool IsAlignedPow2(uint32_t addr, uint32_t pow2) {
  return !(addr & (pow2 - 1));
}

/* Round up to the nearest multiple of pow2 (some power of two). */
static inline uint32_t RoundUpToMultiplePow2(uint32_t x, uint32_t pow2) {
  return (x + pow2 - 1) & (~(pow2 - 1));
}

/*
 * Checks that the region does form a valid interval inside the allocated
 * subspace.
 */
static inline bool IsValidRegion(const MinsfiMemoryRegion *reg,
                                 uint64_t subspace_size, uint32_t page_size) {
  uint64_t region_start = reg->offset;
  uint64_t region_end = region_start + reg->length;

  /*
   * Run the checks. Note that page alignment together with start != end imply
   * that the region is at least one page in size
   */
  return (region_start < region_end) &&
         (region_start <= subspace_size) &&
         (region_end <= subspace_size) &&
         IsAlignedPow2(region_start, page_size) &&
         IsAlignedPow2(region_end, page_size);
}

/*
 * Checks that region1 is followed by region2 with the given gap between them.
 */
static inline bool AreAdjacentRegions(const MinsfiMemoryRegion *region1,
                                      const MinsfiMemoryRegion *region2,
                                      uint32_t gap) {
  return region1->offset + region1->length + gap == region2->offset;
}

static inline uint64_t AddressSubspaceSize(const MinsfiManifest *manifest) {
  return (1LL << manifest->ptr_size);
}

/*
 * Returns the amount of memory actually addressable by the sandbox, i.e. twice
 * the size of the address subspace.
 * See comments in the SandboxMemoryAccessess LLVM pass for more details.
 */
static inline size_t AddressableMemorySize(const MinsfiManifest *manifest) {
  return AddressSubspaceSize(manifest) * 2;
}

bool MinsfiGenerateMemoryLayout(const MinsfiManifest *manifest,
                                uint32_t page_size, MinsfiMemoryLayout *mem) {
  uint64_t subspace_size;

  if (manifest->ptr_size < 20 || manifest->ptr_size > 32 ||
      !IsPowerOfTwo(page_size))
    return false;

  subspace_size = AddressSubspaceSize(manifest);

  /*
   * Data segment is positioned at a fixed offset. The size of the region
   * is rounded to the end of a page.
   */
  mem->dataseg.offset = manifest->dataseg_offset;
  mem->dataseg.length = RoundUpToMultiplePow2(manifest->dataseg_size,
                                              page_size);

  /*
   * Size of the stack is currently a fixed constant, located at the
   * end of the address space.
   */
  mem->stack.length = 32 * page_size;
  mem->stack.offset = subspace_size - mem->stack.length;

  /*
   * Heap fills the space between the data segment and the stack, separated
   * by a guard page at each end. We check that it is at least one page long.
   */
  mem->heap.offset = mem->dataseg.offset + mem->dataseg.length + page_size;
  mem->heap.length = mem->stack.offset - page_size - mem->heap.offset;

  /*
   * Verify that the memory layout is sane. This is important because
   * we do not verify the parameters at the beginning of this function
   * and therefore the values could have overflowed.
   */
  return IsValidRegion(&mem->dataseg, subspace_size, page_size) &&
         IsValidRegion(&mem->heap, subspace_size, page_size) &&
         IsValidRegion(&mem->stack, subspace_size, page_size) &&
         AreAdjacentRegions(&mem->dataseg, &mem->heap, /*gap=*/ page_size) &&
         AreAdjacentRegions(&mem->heap, &mem->stack, /*gap=*/ page_size);
}

/* Change the access rights of a given memory region to read/write. */
static inline bool EnableMemoryRegion(char *mem_base,
                                      const MinsfiMemoryRegion *reg) {
  char *region_base = mem_base + reg->offset;
  return region_base == mmap(region_base, reg->length,
                             PROT_READ | PROT_WRITE,
                             MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
}

bool MinsfiInitSandbox(const MinsfiManifest *manifest, MinsfiSandbox *sb) {
  /*
   * Compute the boundaries of the data segment, heap and stack. Verify
   * that they are sane.
   */
  if (!MinsfiGenerateMemoryLayout(manifest, getpagesize(), &sb->mem_layout))
    return false;

  /* Compute properties of the sandbox */
  sb->mem_alloc_size = AddressableMemorySize(manifest);
  sb->ptr_mask = AddressSubspaceSize(manifest) - 1;

  /* Allocate memory for the sandbox's address subspace */
  sb->mem_base = mmap(NULL, sb->mem_alloc_size,
                      PROT_NONE,
                      MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
  if (sb->mem_base == MAP_FAILED)
    return false;

  /*
   * Change the rights of accessible pages to read/write. Unmap the whole
   * memory region if the operation fails.
   */
  if (!EnableMemoryRegion(sb->mem_base, &sb->mem_layout.dataseg) ||
      !EnableMemoryRegion(sb->mem_base, &sb->mem_layout.heap) ||
      !EnableMemoryRegion(sb->mem_base, &sb->mem_layout.stack)) {
    MinsfiUnmapSandbox(sb);
    return false;
  }

  /* Copy the data segment template into the memory subspace. */
  memcpy(sb->mem_base + sb->mem_layout.dataseg.offset,
         manifest->dataseg_template, manifest->dataseg_size);

  return true;
}

bool MinsfiUnmapSandbox(const MinsfiSandbox *sb) {
  return munmap(sb->mem_base, sb->mem_alloc_size) == 0;
}
