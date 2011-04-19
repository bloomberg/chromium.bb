/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>
#include <string.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/untrusted/irt/irt_elf_utils.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"


void jump_to_elf_start(void *initial_stack_pointer,
                       uintptr_t entry_point,
                       uintptr_t atexit_func);

/*
 * We have a typedef for the size of argc on the stack because this
 * varies between NaCl platforms.
 * See http://code.google.com/p/nativeclient/issues/detail?id=1226
 * TODO(mseaborn): Unify the ABIs.
 */
#if defined(__x86_64__)
typedef int64_t argc_type;
#else
typedef int32_t argc_type;
#endif


static void fatal_error(const char *message) {
  write(2, message, strlen(message));
  _exit(127);
}


struct PP_StartFunctions g_pp_functions;

static void ppapi_start(const struct PP_StartFunctions *funcs) {
  g_pp_functions = *funcs;
  PpapiPluginMain();
}

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  return g_pp_functions.PPP_InitializeModule(module_id, get_browser_interface);
}

void PPP_ShutdownModule() {
  g_pp_functions.PPP_ShutdownModule();
}

const void *PPP_GetInterface(const char *interface_name) {
  return g_pp_functions.PPP_GetInterface(interface_name);
}

/*
 * This overrides the definition in ppapi_proxy/plugin_threading.cc.
 * TODO(mseaborn): Remove this when PPAPI is only supported via the IRT.
 * See http://code.google.com/p/nativeclient/issues/detail?id=1691
 */
void PpapiPluginRegisterDefaultThreadCreator() {
}


static void *query_interface(const char *interface_name) {
  if (strcmp(interface_name, "ppapi_start") == 0) {
    return (void *) (uintptr_t) ppapi_start;
  }
  if (strcmp(interface_name, "ppapi_register_thread_creator") == 0) {
    return (void *) (uintptr_t) PpapiPluginRegisterThreadCreator;
  }
  return NULL;
}


int main(int argc, char **argv) {
  struct auxv_entry *auxv = __find_auxv(argc, argv);
  struct auxv_entry *entry = __find_auxv_entry(auxv, AT_ENTRY);
  if (entry == NULL) {
    fatal_error("irt_entry: No AT_ENTRY item found in auxv.  "
                "Is there no user executable loaded?\n");
  }
  uintptr_t entry_point = entry->a_val;

  /*
   * The user application does not need to see the AT_ENTRY item.
   * Reuse the auxv slot and overwrite it with the PPAPI entry point.
   */
  entry->a_type = AT_SYSINFO;
  entry->a_val = (uintptr_t) query_interface;

  void *stack_pointer = (char *) argv - sizeof(argc_type);
  jump_to_elf_start(stack_pointer, entry_point, 0);

  /* This should never be reached. */
  return 0;
}
