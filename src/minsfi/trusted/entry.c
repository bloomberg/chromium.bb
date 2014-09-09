/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"

/*
 * Fixed offset of the data segment. This must be kept in sync with the
 * AllocateDataSegment compiler pass.
 */
#define DATASEG_OFFSET 0x10000

/* Globals exported by the sandbox, aka the manifest. */
extern uint32_t    __sfi_pointer_size;
extern const char  __sfi_data_segment[];
extern uint32_t    __sfi_data_segment_size;

/* Entry point of the sandbox */
extern uint32_t _start_minsfi(void);

static inline void GetManifest(MinsfiManifest *sb) {
  sb->ptr_size = __sfi_pointer_size;
  sb->dataseg_offset = DATASEG_OFFSET;
  sb->dataseg_size = __sfi_data_segment_size;
  sb->dataseg_template = __sfi_data_segment;
}

bool MinsfiInitializeSandbox(void) {
  MinsfiManifest manifest;
  MinsfiSandbox sb;

  if (MinsfiGetActiveSandbox() != NULL)
    return true;

  GetManifest(&manifest);
  if (!MinsfiInitSandbox(&manifest, &sb))
    return false;

  MinsfiSetActiveSandbox(&sb);
  return true;
}

int MinsfiInvokeSandbox(void) {
  if (MinsfiGetActiveSandbox() == NULL)
    return EXIT_FAILURE;

  return _start_minsfi();
}

bool MinsfiDestroySandbox(void) {
  const MinsfiSandbox *sb;

  if ((sb = MinsfiGetActiveSandbox()) == NULL)
    return true;

  if (MinsfiUnmapSandbox(sb)) {
    MinsfiSetActiveSandbox(NULL);
    return true;
  }

  return false;
}
