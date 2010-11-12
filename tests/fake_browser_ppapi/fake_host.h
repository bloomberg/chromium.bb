/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TEST_FAKE_BROWSER_PPAPI_FAKE_HOST_H_
#define NATIVE_CLIENT_TEST_FAKE_BROWSER_PPAPI_FAKE_HOST_H_

#include <string.h>

#include <map>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_host.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"

struct PPB_Var_Deprecated;

namespace fake_browser_ppapi {

class Host : public ppapi_proxy::BrowserHost {
 public:
  explicit Host(const char* plugin_file);
  virtual ~Host();

  // Implementations of the methods invoked by the browser.
  int32_t InitializeModule(PP_Module module,
                           PPB_GetInterface get_intf);
  void ShutdownModule();
  virtual const void* GetInterface(const char* interface_name);

  void set_var_interface(const PPB_Var_Deprecated* var_interface) {
    var_interface_ = var_interface;
  }
  const PPB_Var_Deprecated* var_interface() const {
    return var_interface_;
  }

  // Resource tracking.
  // The host keeps track of all Resources assigning them a unique resource_id.
  // TrackResource() is to be called right after instantiation of a Resource.
  // GetResource() can be called to map an id to a tracked Resource. If
  // there is no such resource, Resource::Invalid() is returned.
  PP_Resource TrackResource(Resource* resource);
  Resource* GetResource(PP_Resource resource_id);

 private:
  typedef int32_t (*InitializeModuleFunc)(PP_Module module,
                                          PPB_GetInterface get_intf);
  typedef void (*ShutdownModuleFunc)();
  typedef const void* (*GetInterfaceFunc)(const char* interface_name);

  void* dl_handle_;
  InitializeModuleFunc initialize_module_;
  ShutdownModuleFunc shutdown_module_;
  GetInterfaceFunc get_interface_;

  const PPB_Var_Deprecated* var_interface_;

  // Resource tracking.
  PP_Resource last_resource_id_;  // Used and incremented for each new resource.
  typedef std::map<PP_Resource, Resource*> ResourceMap;
  ResourceMap resource_map_;

  NACL_DISALLOW_COPY_AND_ASSIGN(Host);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TEST_FAKE_BROWSER_PPAPI_FAKE_HOST_H_
