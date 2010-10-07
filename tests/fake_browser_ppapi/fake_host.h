/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TEST_FAKE_BROWSER_PPAPI_FAKE_HOST_H_
#define NATIVE_CLIENT_TEST_FAKE_BROWSER_PPAPI_FAKE_HOST_H_

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_host.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"

namespace fake_browser_ppapi {

class Host : public ppapi_proxy::BrowserHost {
 public:
  Host(const char* plugin_file,
       const PPB_Core* core_interface,
       const PPB_Instance* instance_interface,
       const PPB_Var_Deprecated* var_interface);
  virtual ~Host();

  // Implementations of the methods invoked by the browser.
  int32_t InitializeModule(PP_Module module,
                           PPB_GetInterface get_intf);
  void ShutdownModule();
  virtual const void* GetInterface(const char* interface_name);
  // Getters for the browser interfaces.
  const PPB_Core* core_interface() const { return core_interface_; }
  const PPB_Instance* instance_interface() const { return instance_interface_; }
  const PPB_Var_Deprecated* var_interface() const { return var_interface_; }

 private:
  typedef int32_t (*InitializeModuleFunc)(PP_Module module,
                                          PPB_GetInterface get_intf);
  typedef void (*ShutdownModuleFunc)();
  typedef const void* (*GetInterfaceFunc)(const char* interface_name);

  void* dl_handle_;
  InitializeModuleFunc initialize_module_;
  ShutdownModuleFunc shutdown_module_;
  GetInterfaceFunc get_interface_;

  // Store interface pointers.
  const PPB_Core* core_interface_;
  const PPB_Instance* instance_interface_;
  const PPB_Var_Deprecated* var_interface_;

  NACL_DISALLOW_COPY_AND_ASSIGN(Host);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TEST_FAKE_BROWSER_PPAPI_FAKE_HOST_H_
