/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/irt/irt_elf_utils.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"


void jump_to_elf_start(void *initial_stack_pointer,
                       uintptr_t entry_point,
                       uintptr_t atexit_func);

/*
 * We have a typedef for the size of argc on the stack because this
 * varies between NaCl platforms.
 * See http://code.google.com/p/nativeclient/issues/detail?id=1226
 * TODO(mseaborn): Unify the ABIs.
 */
#if defined(__x86_64__)
typedef int64_t argc_type;
#else
typedef int32_t argc_type;
#endif

int main(int argc, char **argv) {
  struct auxv_entry *auxv = __find_auxv(argc, argv);
  struct auxv_entry *entry = __find_auxv_entry(auxv, AT_ENTRY);
  if (entry == NULL) {
    static const char fatal_msg[] =
        "irt_entry: No AT_ENTRY item found in auxv.  "
        "Is there no user executable loaded?\n";
    write(2, fatal_msg, sizeof(fatal_msg) - 1);
    _exit(127);
  }
  uintptr_t entry_point = entry->a_val;

  /*
   * The user application does not need to see the AT_ENTRY item.
   * Reuse the auxv slot and overwrite it with the IRT query function.
   */
  entry->a_type = AT_SYSINFO;
  entry->a_val = (uintptr_t) nacl_irt_interface;

  void *stack_pointer = (char *) argv - sizeof(argc_type);
  jump_to_elf_start(stack_pointer, entry_point, 0);

  /* This should never be reached. */
  return 0;
}
