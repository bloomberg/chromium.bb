/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>
#include <string.h>

#include "native_client/src/include/elf.h"


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

struct auxv_entry {
  uint32_t a_type;
  uint32_t a_val;
};


static void *find_auxv(int argc, char **argv) {
  /* Find the start of the envp array. */
  char **ptr = argv + argc + 1;
  /* Find the end of the envp array. */
  while (*ptr != NULL)
    ptr++;
  return (struct auxv_entry *) (ptr + 1);
}

static struct auxv_entry *find_auxv_entry(struct auxv_entry *auxv,
                                          uint32_t key) {
  for (; auxv->a_type != AT_NULL; auxv++) {
    if (auxv->a_type == key)
      return auxv;
  }
  return NULL;
}

static void fatal_error(const char *message) {
  write(2, message, strlen(message));
  _exit(127);
}

int main(int argc, char **argv) {
  struct auxv_entry *auxv = find_auxv(argc, argv);
  struct auxv_entry *entry = find_auxv_entry(auxv, AT_ENTRY);
  if (entry == NULL) {
    fatal_error("No AT_ENTRY item found in auxv\n");
  }
  void *stack_pointer = (char *) argv - sizeof(argc_type);
  jump_to_elf_start(stack_pointer, entry->a_val, 0);

  /* This should never be reached. */
  return 0;
}
