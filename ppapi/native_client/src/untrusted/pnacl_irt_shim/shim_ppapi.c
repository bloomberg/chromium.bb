/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/shim_ppapi.h"

#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"
#include "ppapi/generators/pnacl_shim.h"
#include "ppapi/native_client/src/shared/ppapi_proxy/ppruntime.h"


/*
 * This is a whitelist of NaCl IRT interfaces that are exposed under
 * PNaCl.  This list omits the following:
 *
 *  * The old versions of "irt-memory", v0.1 and v0.2, which contain
 *    the deprecated sysbrk() function.  See:
 *    https://code.google.com/p/nativeclient/issues/detail?id=3542
 *
 *  * "irt-mutex", "irt-cond" and "irt-sem", which are deprecated and
 *    are superseded by the "irt-futex" interface.  See:
 *    https://code.google.com/p/nativeclient/issues/detail?id=3484
 *
 *  * "irt-dyncode", which is not supported under PNaCl because
 *    dynamically loading architecture-specific native code is not
 *    portable.
 *
 *  * "irt-exception-handling", which is not supported under PNaCl
 *    because it exposes non-portable, architecture-specific register
 *    state.  See:
 *    https://code.google.com/p/nativeclient/issues/detail?id=3444
 *
 *  * "irt-blockhook", which is deprecated.  It was provided for
 *    implementing thread suspension for conservative garbage
 *    collection, but this is probably not a portable use case under
 *    PNaCl, so this interface is disabled under PNaCl.  See:
 *    https://code.google.com/p/nativeclient/issues/detail?id=3539
 *
 *  * "irt-resource-open".  This was primarily provided for use by
 *    nacl-glibc's dynamic linker, which is not supported under PNaCl.
 *    open_resource() returns a file descriptor, but it is the only
 *    interface in NaCl to do so inside Chromium.  This is
 *    inconsistent with PPAPI, which does not expose file descriptors
 *    (except in private/dev interfaces).  See:
 *    https://code.google.com/p/nativeclient/issues/detail?id=3574
 *
 *  * "irt-fdio" and "irt-filename".  Under PNaCl, where
 *    open_resource() open is disallowed, these are only useful for
 *    debugging.  They are only allowed via the "dev" query strings;
 *    the non-"dev" query strings are disallowed.
 *
 * We omit these because they are only "dev" interfaces:
 *
 *  * "irt-dev-getpid"
 *  * "irt-dev-list-mappings"
 */
static const char *const irt_interface_whitelist[] = {
  NACL_IRT_BASIC_v0_1,
  NACL_IRT_MEMORY_v0_3,
  NACL_IRT_THREAD_v0_1,
  NACL_IRT_FUTEX_v0_1,
  NACL_IRT_TLS_v0_1,
  NACL_IRT_PPAPIHOOK_v0_1,
  NACL_IRT_RANDOM_v0_1,
  NACL_IRT_CLOCK_v0_1,
  /* Allowed for debugging purposes: */
  NACL_IRT_DEV_FDIO_v0_1,
  NACL_IRT_DEV_FILENAME_v0_2,
};

/* Use local strcmp to avoid dependency on libc. */
static int mystrcmp(const char* s1, const char *s2) {
  while((*s1 && *s2) && (*s1++ == *s2++));
  return *(--s1) - *(--s2);
}

static int is_irt_interface_whitelisted(const char *interface_name) {
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(irt_interface_whitelist); i++) {
    if (mystrcmp(interface_name, irt_interface_whitelist[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

TYPE_nacl_irt_query __pnacl_real_irt_interface;

/*
 * These remember the interface pointers the user registers by calling the
 * IRT entry point.
 */
static struct PP_StartFunctions user_start_functions;

static int32_t wrap_PPPInitializeModule(PP_Module module_id,
                                        PPB_GetInterface get_browser_intf) {
  __set_real_Pnacl_PPBGetInterface(get_browser_intf);
  /*
   * Calls from user code to the PPB interfaces pass through here and may
   * require shims to convert the ABI.
   */
  return (*user_start_functions.PPP_InitializeModule)(module_id,
                                                      &__Pnacl_PPBGetInterface);
}

static void wrap_PPPShutdownModule() {
  (*user_start_functions.PPP_ShutdownModule)();
}

static const struct PP_StartFunctions wrapped_ppapi_methods = {
  wrap_PPPInitializeModule,
  wrap_PPPShutdownModule,
  /*
   * Calls from the IRT to the user plugin pass through here and may require
   * shims to convert the ABI.
   */
  __Pnacl_PPPGetInterface
};

static struct nacl_irt_ppapihook real_irt_ppapi_hook;

static int wrap_ppapi_start(const struct PP_StartFunctions *funcs) {
  /*
   * Save the user's real bindings for the start functions.
   */
  user_start_functions = *funcs;
  __set_real_Pnacl_PPPGetInterface(user_start_functions.PPP_GetInterface);

  /*
   * Invoke the IRT's ppapi_start interface with the wrapped interface.
   */
  return (*real_irt_ppapi_hook.ppapi_start)(&wrapped_ppapi_methods);
}

size_t __pnacl_irt_interface_wrapper(const char *interface_ident,
                                     void *table, size_t tablesize) {
  if (!is_irt_interface_whitelisted(interface_ident))
    return 0;

  /*
   * Note there is a benign race in initializing the wrapper.
   * We build the "hook" structure by copying from the IRT's hook and then
   * writing our wrapper for the ppapi method.  Two threads may end up
   * attempting to do this simultaneously, which should not be a problem,
   * as they are writing the same values.
   */
  if (0 != mystrcmp(interface_ident, NACL_IRT_PPAPIHOOK_v0_1)) {
    /*
     * The interface is not wrapped, so use the real interface.
     */
    return (*__pnacl_real_irt_interface)(interface_ident, table, tablesize);
  }
  if ((*__pnacl_real_irt_interface)(NACL_IRT_PPAPIHOOK_v0_1,
                                    &real_irt_ppapi_hook,
                                    sizeof real_irt_ppapi_hook) !=
      sizeof real_irt_ppapi_hook) {
    return 0;
  }
  /*
   * Copy the interface structure into the client.
   */
  struct nacl_irt_ppapihook *dest = table;
  if (sizeof *dest <= tablesize) {
    dest->ppapi_start = wrap_ppapi_start;
    dest->ppapi_register_thread_creator =
        real_irt_ppapi_hook.ppapi_register_thread_creator;
    return sizeof *dest;
  }
  return 0;
}
