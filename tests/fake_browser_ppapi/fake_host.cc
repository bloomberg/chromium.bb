/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_host.h"

#if !NACL_WINDOWS
# include <dlfcn.h>
#endif  // !NACL_WINDOWS
#include <string.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"

namespace {

#if NACL_WINDOWS

#define RTLD_NOW 0
#define RTLD_LOCAL 0

void* dlopen(const char* filename, int flag) {
  return reinterpret_cast<void*>(LoadLibrary(filename));
}

void* dlsym(void *handle, const char* symbol_name) {
  return GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name);
}

int dlclose(void* handle) {
  return !FreeLibrary(reinterpret_cast<HMODULE>(handle));
}

#endif

}  // namespace


namespace fake_browser_ppapi {

Host::Host(const char* plugin_file,
           const PPB_Core* core_interface,
           const PPB_Instance* instance_interface,
           const PPB_Var* var_interface) :
  core_interface_(core_interface),
  instance_interface_(instance_interface),
  var_interface_(var_interface) {
  dl_handle_ = dlopen(plugin_file, RTLD_NOW | RTLD_LOCAL);
  CHECK(dl_handle_ != NULL);
  initialize_module_ =
      reinterpret_cast<InitializeModuleFunc>(
        reinterpret_cast<uintptr_t>(dlsym(dl_handle_, "PPP_InitializeModule")));
  CHECK(initialize_module_ != NULL);
  shutdown_module_ =
      reinterpret_cast<ShutdownModuleFunc>(
        reinterpret_cast<uintptr_t>(dlsym(dl_handle_, "PPP_ShutdownModule")));
  CHECK(shutdown_module_ != NULL);
  get_interface_ =
      reinterpret_cast<GetInterfaceFunc>(
        reinterpret_cast<uintptr_t>(dlsym(dl_handle_, "PPP_GetInterface")));
  CHECK(get_interface_ != NULL);
}

Host::~Host() {
  int rc = dlclose(dl_handle_);
  CHECK(rc == 0);
}

int32_t Host::InitializeModule(PP_Module module,
                               PPB_GetInterface get_intf) {
  return (*initialize_module_)(module, get_intf);
}

void Host::ShutdownModule() {
  return (*shutdown_module_)();
}

const void* Host::GetInterface(const char* interface_name) {
  return (*get_interface_)(interface_name);
}

}  // namespace fake_browser_ppapi
