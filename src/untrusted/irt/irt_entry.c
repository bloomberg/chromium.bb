/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/public/irt_core.h"

#include <assert.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"
#include "native_client/src/untrusted/nacl/tls.h"

void __libc_init_array(void);

void nacl_irt_init(uint32_t *info) {
  void (*fini)(void) = nacl_startup_fini(info);
  char **envp = nacl_startup_envp(info);

  environ = envp;

  /*
   * We are the true entry point, never called by a dynamic linker.
   * So the finalizer function pointer is always NULL.
   * We don't bother registering anything with atexit anyway,
   * since we do not expect our own exit function ever to be called.
   * Any cleanup we might need done must happen in nacl_irt_exit (irt_basic.c).
   */
  assert(fini == NULL);

  __pthread_initialize();

  __libc_init_array();

  /*
   * SRPC is initialized for use by irt_nameservice.c, which is used
   * by irt_random.c, and (in Chromium) by irt_manifest.c.
   */
  if (!NaClSrpcModuleInit()) {
    static const char fatal_msg[] = "NaClSrpcModuleInit() failed\n";
    write(2, fatal_msg, sizeof(fatal_msg) - 1);
    _exit(-1);
  }
  NaClLogModuleInit();  /* Enable NaClLog'ing used by CHECK(). */
}

void nacl_irt_enter_user_code(uint32_t *info,
                              nacl_irt_query_func_t query_func) {
  Elf32_auxv_t *auxv = nacl_startup_auxv(info);
  Elf32_auxv_t *entry = NULL;
  for (Elf32_auxv_t *av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_ENTRY) {
      entry = av;
      break;
    }
  }
  if (entry == NULL) {
    static const char fatal_msg[] =
        "irt_entry: No AT_ENTRY item found in auxv.  "
        "Is there no user executable loaded?\n";
    write(2, fatal_msg, sizeof(fatal_msg) - 1);
    _exit(-1);
  }
  void (*user_start)(uint32_t *) = (void (*)(uint32_t *)) entry->a_un.a_val;

  /*
   * The user application does not need to see the AT_ENTRY item.
   * Reuse the auxv slot and overwrite it with the IRT query function.
   */
  entry->a_type = AT_SYSINFO;
  entry->a_un.a_val = (uintptr_t) query_func;

  /*
   * Call the user entry point function.  It should not return.
   */
  (*user_start)(info);

  /*
   * But just in case it does...
   */
  _exit(0);
}

/* This is the true entry point for untrusted code. */
void _start(uint32_t *info) {
  nacl_irt_start(info);
}

/*
 * This is never actually called, but there is an artificial undefined
 * reference to it.
 */
int main(void) {
}
