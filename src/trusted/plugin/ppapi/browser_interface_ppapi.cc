/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>

#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/var_utils.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"

using nacl::assert_cast;

namespace {

bool GetWindow(plugin::InstanceIdentifier instance_id, pp::Var* window) {
  pp::Instance* instance = plugin::InstanceIdentifierToPPInstance(instance_id);
  *window = instance->GetWindowObject();
  return !window->is_void();
}

}  // namespace

namespace plugin {

uintptr_t BrowserInterfacePpapi::StringToIdentifier(const nacl::string& str) {
  StringToIdentifierMap::iterator iter = string_to_identifier_map_.find(str);
  if (iter == string_to_identifier_map_.end()) {
    uintptr_t id = reinterpret_cast<uintptr_t>(str.c_str());
    string_to_identifier_map_.insert(make_pair(str, id));
    identifier_to_string_map_.insert(make_pair(id, str));
    return id;
  }
  return string_to_identifier_map_[str];
}


nacl::string BrowserInterfacePpapi::IdentifierToString(uintptr_t ident) {
  assert(identifier_to_string_map_.find(ident) !=
         identifier_to_string_map_.end());
  return identifier_to_string_map_[ident];
}


bool BrowserInterfacePpapi::Alert(InstanceIdentifier instance_id,
                                  const nacl::string& text) {
  pp::Var window;
  if (!GetWindow(instance_id, &window)) {
    return false;
  }
  pp::Var exception;
  window.Call("alert", text, &exception);
  return exception.is_void();
}


bool BrowserInterfacePpapi::EvalString(InstanceIdentifier instance_id,
                                       const nacl::string& expression) {
  pp::Var window;
  if (!GetWindow(instance_id, &window)) {
    return false;
  }
  pp::Var exception;
  window.Call("eval", expression, &exception);
  return exception.is_void();
}


bool BrowserInterfacePpapi::GetFullURL(InstanceIdentifier instance_id,
                                       nacl::string* full_url) {
  *full_url = kUnknownURL;
  pp::Var window;
  if (!GetWindow(instance_id, &window)) {
    PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (get window failed)\n"));
    return false;
  }
  pp::Var location = window.GetProperty("location");
  PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (location=%s)\n",
                 VarToString(location).c_str()));
  if (location.is_object()) {
    pp::Var href = location.GetProperty("href");
    PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (href=%s)\n",
                   VarToString(href).c_str()));
    if (href.is_string()) {
      *full_url = href.AsString();
    }
  }
  PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (full_url='%s')\n",
                 full_url->c_str()));
  return (kUnknownURL != *full_url);
}


ScriptableHandle* BrowserInterfacePpapi::NewScriptableHandle(
    PortableHandle* handle) {
  return ScriptableHandlePpapi::New(handle);
}


pp::Instance* InstanceIdentifierToPPInstance(InstanceIdentifier instance_id) {
  return reinterpret_cast<pp::Instance*>(assert_cast<intptr_t>(instance_id));
}


InstanceIdentifier PPInstanceToInstanceIdentifier(pp::Instance* instance) {
  return assert_cast<InstanceIdentifier>(reinterpret_cast<intptr_t>(instance));
}

}  // namespace plugin
