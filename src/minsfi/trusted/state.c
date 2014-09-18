/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <string.h>

#include "native_client/src/include/minsfi_priv.h"

/*
 * This is the memory base variable which the SFI-sandboxed pointers are
 * treated relative to. It is defined in the trusted runtime, exported and then
 * imported by the untrusted code via the compiler transforms. It is a 64-bit
 * unsigned integer on both 32-bit and 64-bit platforms. See comments in the
 * SandboxMemoryAccesses MinSFI LLVM pass for more details.
 */
uint64_t __sfi_memory_base = 0;

static MinsfiSandbox ActiveSandbox = { .mem_base = NULL };

MinsfiSandbox *MinsfiGetActiveSandbox(void) {
  if (ActiveSandbox.mem_base != NULL)
    return &ActiveSandbox;
  else
    return NULL;
}

void MinsfiSetActiveSandbox(const MinsfiSandbox *sb) {
  if (sb != NULL)
    memcpy(&ActiveSandbox, sb, sizeof(MinsfiSandbox));
  else
    ActiveSandbox.mem_base = NULL;

  __sfi_memory_base = (uintptr_t) ActiveSandbox.mem_base;
}
