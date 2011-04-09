/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/irt/irt_elf_utils.h"


void *__find_auxv(int argc, char **argv) {
  /* Find the start of the envp array. */
  char **ptr = argv + argc + 1;
  /* Find the end of the envp array. */
  while (*ptr != NULL)
    ptr++;
  return (struct auxv_entry *) (ptr + 1);
}

struct auxv_entry *__find_auxv_entry(struct auxv_entry *auxv, uint32_t key) {
  for (; auxv->a_type != AT_NULL; auxv++) {
    if (auxv->a_type == key)
      return auxv;
  }
  return NULL;
}
