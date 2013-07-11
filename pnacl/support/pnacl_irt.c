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

static TYPE_nacl_irt_query g_irt_query_func;

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

/* Returns whether |str| starts with |prefix|. */
static int starts_with(const char *str, const char *prefix) {
  while (*prefix != '\0') {
    if (*str++ != *prefix++)
      return 0;
  }
  return 1;
}

/*
 * This wraps the IRT interface query function in order to hide IRT
 * interfaces that are disallowed under PNaCl.
 */
static size_t irt_query_filter(const char *interface_ident,
                               void *table, size_t tablesize) {
  static const char common_prefix[] = "nacl-irt-";
  if (starts_with(interface_ident, common_prefix)) {
    const char *rest = interface_ident + sizeof(common_prefix) - 1;
    /*
     * "irt-mutex" and "irt-cond" are deprecated and are superseded by
     * the "irt-futex" interface.
     * See https://code.google.com/p/nativeclient/issues/detail?id=3484
     */
    if (starts_with(rest, "mutex-") ||
        starts_with(rest, "cond-")) {
      return 0;
    }
    /*
     * "irt-blockhook" is deprecated.  It was provided for
     * implementing thread suspension for conservative garbage
     * collection, but this is probably not a portable use case under
     * PNaCl, so this interface is disabled under PNaCl.
     * See https://code.google.com/p/nativeclient/issues/detail?id=3539
     */
    if (starts_with(rest, "blockhook-"))
      return 0;
  }
  return g_irt_query_func(interface_ident, table, tablesize);
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
      /* Replace the IRT interface query function with a wrapper. */
      g_irt_query_func = irt_query;
      av->a_un.a_val = (uintptr_t) irt_query_filter;
      return;
    }
  }
}
