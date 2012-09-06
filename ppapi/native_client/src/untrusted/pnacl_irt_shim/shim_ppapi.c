/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/pnacl_irt_shim/shim_ppapi.h"

#include <string.h>
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"
#include "ppapi/generators/pnacl_shim.h"

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

static struct nacl_irt_ppapihook ppapi_hook = {
  wrap_ppapi_start,
  NULL
};

size_t __pnacl_irt_interface_wrapper(const char *interface_ident,
                                     void *table, size_t tablesize) {
  /*
   * Note there is a benign race in initializing the wrapper.
   * We build the "hook" structure by copying from the IRT's hook and then
   * writing our wrapper for the ppapi method.  Two threads may end up
   * attempting to do this simultaneously, which should not be a problem,
   * as they are writing the same values.
   */
  if (0 != strcmp(interface_ident, NACL_IRT_PPAPIHOOK_v0_1)) {
    /*
     * The interface is not wrapped, so use the real interface.
     */
    return (*__pnacl_real_irt_interface)(interface_ident, table, tablesize);
  }
  if ((*__pnacl_real_irt_interface)(NACL_IRT_PPAPIHOOK_v0_1,
                                    &real_irt_ppapi_hook,
                                    sizeof real_irt_ppapi_hook) !=
      sizeof real_irt_ppapi_hook) {
    return -1;
  }
  /*
   * Copy the portion of the ppapihook interface that is not wrapped.
   */
  ppapi_hook.ppapi_register_thread_creator =
      real_irt_ppapi_hook.ppapi_register_thread_creator;
  /*
   * Copy the interface structure into the client.
   */
  if (sizeof ppapi_hook <= tablesize) {
    memcpy(table, &ppapi_hook, sizeof ppapi_hook);
    return sizeof ppapi_hook;
  }
  return 0;
}
