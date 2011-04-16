// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements the required interfaces for representing a plugin module
// instance in browser interactions and provides a way to register custom
// plugin interfaces.
//

#include <stdio.h>
#include <string.h>

#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/ppapi_test_lib/internal_utils.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"

///////////////////////////////////////////////////////////////////////////////
// Plugin interface registration
///////////////////////////////////////////////////////////////////////////////

namespace {

class PluginInterfaceTable {
 public:
  // Return singleton intsance.
  static PluginInterfaceTable* Get() {
    static PluginInterfaceTable table;
    return &table;
  }

  void AddInterface(const char* interface_name, const void* ppp_interface) {
    interface_map_[nacl::string(interface_name)] = ppp_interface;
  }
  const void* GetInterface(const char* interface_name) {
    // This will add a NULL element for missing interfaces.
    return interface_map_[nacl::string(interface_name)];
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginInterfaceTable);

  PluginInterfaceTable() {}

  typedef std::map<nacl::string, const void*> InterfaceMap;
  InterfaceMap interface_map_;
};

}  // namespace

void RegisterPluginInterface(const char* interface_name,
                             const void* ppp_interface) {
  PluginInterfaceTable::Get()->AddInterface(interface_name, ppp_interface);
}


///////////////////////////////////////////////////////////////////////////////
// PPP_Instance default implementation
///////////////////////////////////////////////////////////////////////////////

namespace {

PP_Bool DidCreate(PP_Instance instance,
                  uint32_t /*argc*/,
                  const char* /*argn*/[],
                  const char* /*argv*/[]) {
  set_pp_instance(instance);
  return PP_TRUE;
}

void DidDestroy(PP_Instance /*instance*/) {
}

void DidChangeView(PP_Instance /*instance*/,
                   const struct PP_Rect* /*position*/,
                   const struct PP_Rect* /*clip*/) {
}

void DidChangeFocus(PP_Instance /*instance*/,
                    PP_Bool /*has_focus*/) {
}

PP_Bool HandleInputEvent(PP_Instance /*instance*/,
                         const struct PP_InputEvent* /*event*/) {
  return PP_TRUE;
}

PP_Bool HandleDocumentLoad(PP_Instance instance,
                           PP_Resource url_loader) {
  return PP_TRUE;
}

PP_Var GetInstanceObject(PP_Instance instance) {
  return GetScriptableObject(instance);
}

const PPP_Instance default_ppp_instance_interface = {
  DidCreate,
  DidDestroy,
  DidChangeView,
  DidChangeFocus,
  HandleInputEvent,
  HandleDocumentLoad,
  GetInstanceObject
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PPP implementation
///////////////////////////////////////////////////////////////////////////////

int32_t PPP_InitializeModule(PP_Module module,
                             PPB_GetInterface get_browser_interface) {
  set_pp_module(module);
  set_ppb_get_interface(get_browser_interface);
  SetupPluginInterfaces();
  return PP_OK;
}

void PPP_ShutdownModule() {
}

const void* PPP_GetInterface(const char* interface_name) {
  // This PPP_Instance interface is required, so we provide the default.
  if (0 == strncmp(PPP_INSTANCE_INTERFACE, interface_name,
                   strlen(PPP_INSTANCE_INTERFACE))) {
    return &default_ppp_instance_interface;
  }

  return PluginInterfaceTable::Get()->GetInterface(interface_name);
}
