/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PRIV_H_
#define NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PRIV_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

typedef struct {
  uint32_t ptr_size;               /* size of sandboxed pointers in bits */
  uint32_t dataseg_offset;
  uint32_t dataseg_size;
  const char *dataseg_template;
} MinsfiManifest;

typedef struct {
  uint32_t offset;
  uint32_t length;
} MinsfiMemoryRegion;

typedef struct {
  MinsfiMemoryRegion dataseg;
  MinsfiMemoryRegion heap;
  MinsfiMemoryRegion stack;
} MinsfiMemoryLayout;

typedef struct {
  char *mem_base;
  uint64_t mem_alloc_size;
  uint32_t ptr_mask;
  MinsfiMemoryLayout mem_layout;
} MinsfiSandbox;

/*
 * Computes the boundaries of the individual regions of the sandbox's address
 * subspace and stores them into the given minsfi_layout data structure.
 * Returns FALSE if a layout cannot be created for the given parameters.
 */
bool MinsfiGenerateMemoryLayout(const MinsfiManifest *manifest,
                                uint32_t page_size, MinsfiMemoryLayout *layout);

/*
 * This function initializes the address subspace of the sandbox. Protection of
 * the pages allocated to the data segment, heap and stack is set to read/write,
 * the rest is no-access. The data segment template is copied into the sandbox.
 *
 * The function returns TRUE if the initialization was successful, and stores
 * information about the sandbox into the given MinsfiSandbox struct.
 */
bool MinsfiInitSandbox(const MinsfiManifest *manifest, MinsfiSandbox *sb);

/*
 * Unmaps a memory region given by the provided base and the declared pointer
 * size of the sandbox. The function returns FALSE if munmap() fails.
 */
bool MinsfiUnmapSandbox(const MinsfiSandbox *sb);

/*
 * Returns information about the active sandbox, or NULL if there is no
 * initialized sandbox at the moment.
 */
const MinsfiSandbox *MinsfiGetActiveSandbox(void);

/*
 * Sets the sandbox which all trampolines will refer to. Internally copies the
 * data structure to its own storage.
 */
void MinsfiSetActiveSandbox(const MinsfiSandbox *sb);

#endif  // NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PRIV_H_
