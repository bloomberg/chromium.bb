/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PRIV_H_
#define NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PRIV_H_

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * An integer type capable of holding an address converted from an untrusted
 * pointer. Functions in the minsfi_ptr.h header file convert between
 * native and untrusted pointers without loss of information.
 */
typedef uint32_t sfiptr_t;

typedef struct {
  uint32_t ptr_size;               /* size of sandboxed pointers in bits */
  uint32_t dataseg_offset;
  uint32_t dataseg_size;
  const char *dataseg_template;
} MinsfiManifest;

typedef struct {
  sfiptr_t offset;
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
  sfiptr_t ptr_mask;
  MinsfiMemoryLayout mem_layout;
  jmp_buf exit_env;
  int32_t exit_code;
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
 * Arguments are passed to the sandbox with a single pointer to an array of
 * integers called 'info' where:
 *   info[0] = argc
 *   info[j+1] = untrusted pointer to argv[j]      (for 0 <= j < argc)
 * The sandbox will expect this array to be stored at the bottom of the
 * untrusted stack and will start growing the stack backwards from the given
 * address.
 *
 * This function will iterate over the arguments, store the argv[*] strings
 * at the bottom of the untrusted stack and prepend it with the 'info' data
 * structure as described above.
 */
sfiptr_t MinsfiCopyArguments(int argc, char *argv[], const MinsfiSandbox *sb);

/*
 * Unmaps a memory region given by the provided base and the declared pointer
 * size of the sandbox. The function returns FALSE if munmap() fails.
 */
bool MinsfiUnmapSandbox(const MinsfiSandbox *sb);

/*
 * Returns information about the active sandbox, or NULL if there is no
 * initialized sandbox at the moment.
 */
MinsfiSandbox *MinsfiGetActiveSandbox(void);

/*
 * Sets the sandbox which all trampolines will refer to. Internally copies the
 * data structure to its own storage.
 */
void MinsfiSetActiveSandbox(const MinsfiSandbox *sb);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PRIV_H_
