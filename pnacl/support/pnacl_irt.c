/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"


/*
 * TODO(mseaborn): Use the definition in nacl_config.h instead.
 * nacl_config.h is not #includable here because NACL_BUILD_ARCH
 * etc. are not defined at this point in the PNaCl toolchain build.
 */
#define NACL_SYSCALL_ADDR(syscall_number) (0x10000 + (syscall_number) * 32)

/*
 * If we are not running under the IRT, we fall back to using the
 * syscall directly.  This is to support non-IRT-based tests and the
 * non-IRT-using sandboxed PNaCl translator.
 */
static void *(*g_nacl_read_tp_func)(void) =
    (void *(*)(void)) NACL_SYSCALL_ADDR(NACL_sys_tls_get);

/*
 * __nacl_read_tp is defined as a weak symbol because if a pre-translated
 * object file (which may contain calls to __nacl_read_tp) is linked with
 * the bitcode libnacl in nonpexe_tests, it will pull in libnacl's definition,
 * which would then override this one at native link time rather than causing
 * link failure.
 */
__attribute__((weak))
void *__nacl_read_tp(void) {
  return g_nacl_read_tp_func();
}

void __pnacl_init_irt(uint32_t *startup_info) {
  Elf32_auxv_t *av = nacl_startup_auxv(startup_info);

  for (; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO) {
      TYPE_nacl_irt_query irt_query = (TYPE_nacl_irt_query) av->a_un.a_val;
      struct nacl_irt_tls irt_tls;
      if (irt_query(NACL_IRT_TLS_v0_1, &irt_tls, sizeof(irt_tls))
          == sizeof(irt_tls)) {
        g_nacl_read_tp_func = irt_tls.tls_get;
      }
      return;
    }
  }
}
