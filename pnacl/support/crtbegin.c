/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is the native crtbegin (crtbegin.o).
 *
 * It contains the constructor/destructor for exception handling,
 * and the symbol for __EH_FRAME_BEGIN__. This is native because
 * exception handling is also native (externally provided).
 *
 * One x86-64 we also need to sneak in the native shim library
 * between these bookends because otherwise the unwind tables will be
 * messed up, c.f.
 * https://chromiumcodereview.appspot.com/9909016/
 * TODO(robertm): see whether this problem goes away once we switch
 * eh_frame_headers
 */

#include <stdint.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"

void _pnacl_wrapper_start(uint32_t *info);
void _start(uint32_t *info);

/*
 * HACK:
 * The real structure is defined in unwind-dw2-fde.h
 * this is something that is at least twice as big.
 */
struct object {
  void *p[16] __attribute__((aligned(8)));
};

/*
 * __[de]register_frame_info() are provided by libgcc_eh
 * See: gcc/unwind-dw2-fde.c
 * GCC uses weak linkage to make this library optional, but
 * we always link it in.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[2] */
extern void __register_frame_info(void *begin, struct object *ob);
extern void __deregister_frame_info(const void *begin);

/*
 * Exception handling frames are aggregated into a single section called
 * .eh_frame.  The runtime system needs to (1) have a symbol for the beginning
 * of this section, and needs to (2) mark the end of the section by a NULL.
 */

static char __EH_FRAME_BEGIN__[]
    __attribute__((section(".eh_frame"), aligned(4)))
    = { };


#if defined(SHARED)

/*
 * Registration and deregistration of exception handling tables are done
 * by .init_array and .fini_array elements added here.
 */
/*
 * __attribute__((constructor)) places a call to the function in the
 * .init_array section in PNaCl.  The function pointers in .init_array
 * are then invoked in order (__do_eh_ctor is invoked first) before main.
 */
static void __attribute__((constructor)) __do_eh_ctor(void) {
  static struct object object;
  __register_frame_info (__EH_FRAME_BEGIN__, &object);
}

/*
 * __attribute__((destructor)) places a call to the function in the
 * .fini_array section in PNaCl.  The function pointers in .fini_array
 * are then invoked in inverse order (__do_global_dtors_aux is invoked last)
 * at exit.
 */
static void __attribute__((destructor)) __do_eh_dtor(void) {
  __deregister_frame_info (__EH_FRAME_BEGIN__);
}

#else

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

static void read_auxv(const Elf32_auxv_t *auxv) {
  const Elf32_auxv_t *av;
  for (av = auxv; av->a_type != AT_NULL; ++av) {
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

static void __do_eh_ctor(void) {
  static struct object object;
  __register_frame_info (__EH_FRAME_BEGIN__, &object);
}

/* This defines the entry point for a nexe produced by the PNaCl translator. */
void __pnacl_start(uint32_t *info) {
  read_auxv(nacl_startup_auxv(info));

  /*
   * We must register exception handling unwind info before calling
   * any user code.  Note that we do not attempt to deregister the
   * unwind info at exit, but there is no particular need to do so.
   */
  __do_eh_ctor();

  _pnacl_wrapper_start(info);
}

#endif
