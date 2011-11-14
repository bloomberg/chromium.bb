/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/pnacl_irt_shim/shim_ppapi.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"


/*
 * This is the true entry point for untrusted code.
 * See nacl_startup.h for the layout at the argument pointer.
 */
void _pnacl_wrapper_start(uint32_t *info) {
  Elf32_auxv_t *auxv = nacl_startup_auxv(info);

  Elf32_auxv_t *entry = NULL;
  for (Elf32_auxv_t *av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO) {
      entry = av;
      break;
    }
  }
  if (entry == NULL) {
    static const char fatal_msg[] =
        "pnacl_wrapper_start: No AT_SYSINFO item found in auxv.\n";
    write(2, fatal_msg, sizeof(fatal_msg) - 1);
    exit(-1);
  }

  /*
   * Save the real irt interface.
   */
  __pnacl_real_irt_interface = (TYPE_nacl_irt_query) entry->a_un.a_val;

  /*
   * Overwrite the auxv slot with the pnacl IRT shim query function.
   */
  entry->a_type = AT_SYSINFO;
  entry->a_un.a_val = (uintptr_t) __pnacl_irt_interface_wrapper;

  /*
   * Call the user entry point function.  It should not return.
   * TODO(sehr): Find a way to ensure this is invoked via a tail call.
   */
  _start(info);
}
