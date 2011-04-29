/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_elf_utils.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"
#include "native_client/src/untrusted/irt_stub/thread_creator.h"


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
  TYPE_nacl_irt_query query_func = (TYPE_nacl_irt_query) entry->a_val;
  struct nacl_irt_ppapihook hooks;
  if (sizeof(hooks) != query_func(NACL_IRT_PPAPIHOOK_v0_1,
                                  &hooks, sizeof(hooks))) {
    fatal_error("plugin_main_irt: ppapi hooks not found\n");
  }
  __nacl_register_thread_creator(&hooks);
  hooks.ppapi_start(&funcs);
  return 1;
}
