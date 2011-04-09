/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/irt/irt_elf_utils.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"


static void fatal_error(const char *message) {
  write(2, message, strlen(message));
  _exit(127);
}


const static struct PP_StartFunctions funcs = {
  PPP_InitializeModule,
  PPP_ShutdownModule,
  PPP_GetInterface
};

int main(int argc, char **argv) {
  struct auxv_entry *auxv = __find_auxv(argc, argv);
  struct auxv_entry *entry = __find_auxv_entry(auxv, AT_SYSINFO);
  if (entry == NULL) {
    fatal_error("plugin_main_irt: No AT_SYSINFO item found in auxv, "
                "so cannot start PPAPI.  Is the IRT library not present?\n");
  }
  PP_StartFunc pp_start = (PP_StartFunc) entry->a_val;
  pp_start(&funcs);
  return 1;
}
