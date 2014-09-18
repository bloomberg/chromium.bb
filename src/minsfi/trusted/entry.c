/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"
#include "native_client/src/include/minsfi_ptr.h"

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
extern uint32_t _start_minsfi(sfiptr_t info);

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

sfiptr_t MinsfiCopyArguments(int argc, char *argv[], const MinsfiSandbox *sb) {
  int arg_index;
  size_t arg_length, info_length;
  sfiptr_t *info;
  sfiptr_t stack_base, stack_ptr;

  if (argc < 0)
    return 0;

  /* Allocate memory for the info data structure. */
  info_length = (argc + 1) * sizeof(sfiptr_t);
  info = (sfiptr_t*) malloc(info_length);
  info[0] = argc;

  /* Compute the bounds of the stack. */
  stack_base = sb->mem_layout.stack.offset;
  stack_ptr = stack_base + sb->mem_layout.stack.length;

  /* Copy the argv[*] strings onto the stack. Return NULL if the stack is not
   * large enough. */
  for (arg_index = 0; arg_index < argc; ++arg_index) {
    arg_length = strlen(argv[arg_index]) + 1;
    stack_ptr -= arg_length;
    if (stack_ptr < stack_base) {
      free(info);
      return 0;
    }

    MinsfiCopyToSandbox(stack_ptr, argv[arg_index], arg_length, sb);
    info[arg_index + 1] = stack_ptr;
  }

  /* Copy the info data structure across. */
  stack_ptr -= info_length;
  if (stack_ptr < stack_base) {
    free(info);
    return 0;
  }
  MinsfiCopyToSandbox(stack_ptr, info, info_length, sb);

  /* Clean up. */
  free(info);

  /* Return untrusted pointer to the beginning of the data structure. */
  return stack_ptr;
}

int MinsfiInvokeSandbox(int argc, char *argv[]) {
  MinsfiSandbox *sb;
  sfiptr_t info;

  if ((sb = MinsfiGetActiveSandbox()) == NULL)
    return EXIT_FAILURE;

  if ((info = MinsfiCopyArguments(argc, argv, sb)) == 0)
    return EXIT_FAILURE;

  if (setjmp(sb->exit_env) == 0) {
    _start_minsfi(info);
    return EXIT_FAILURE;
  } else {
    return sb->exit_code;
  }
}

bool MinsfiDestroySandbox(void) {
  const MinsfiSandbox *sb;

  if ((sb = MinsfiGetActiveSandbox()) == NULL)
    return true;

  /*
   * TODO: This function should close the file descriptors held by the sandbox
   *       once I/O syscalls are implemented.
   */

  if (MinsfiUnmapSandbox(sb)) {
    MinsfiSetActiveSandbox(NULL);
    return true;
  }

  return false;
}
