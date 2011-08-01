// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/elf.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"

#include "native_client/src/third_party/ppapi/cpp/private/instance_private.h"
#include "native_client/src/third_party/ppapi/cpp/private/var_private.h"

using nacl::assert_cast;

namespace plugin {

namespace {

pp::VarPrivate GetWindow(plugin::InstanceIdentifier instance_id) {
  pp::InstancePrivate* instance = InstanceIdentifierToPPInstance(instance_id);
  return instance->GetWindowObject();
}

}  // namespace

uintptr_t BrowserInterfacePpapi::StringToIdentifier(const nacl::string& str) {
  StringToIdentifierMap::iterator iter = string_to_identifier_map_.find(str);
  if (iter == string_to_identifier_map_.end()) {
    uintptr_t id = next_identifier++;
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
  pp::Var exception;
  GetWindow(instance_id).Call("alert", text, &exception);
  return exception.is_undefined();
}

bool BrowserInterfacePpapi::AddToConsole(InstanceIdentifier instance_id,
                                         const nacl::string& text) {
  pp::Var exception;
  pp::VarPrivate window = GetWindow(instance_id);
  window.GetProperty("console", &exception).Call("log", text, &exception);
  return exception.is_undefined();
}

bool BrowserInterfacePpapi::EvalString(InstanceIdentifier instance_id,
                                       const nacl::string& expression) {
  pp::Var exception;
  GetWindow(instance_id).Call("eval", expression, &exception);
  return exception.is_undefined();
}


bool BrowserInterfacePpapi::GetFullURL(InstanceIdentifier instance_id,
                                       nacl::string* full_url) {
  *full_url = NACL_NO_URL;
  pp::VarPrivate location = GetWindow(instance_id).GetProperty("location");
  PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (location=%s)\n",
                 location.DebugString().c_str()));
  pp::VarPrivate href = location.GetProperty("href");
  PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (href=%s)\n",
                 href.DebugString().c_str()));
  if (href.is_string()) {
    *full_url = href.AsString();
  }
  PLUGIN_PRINTF(("BrowserInterfacePpapi::GetFullURL (full_url='%s')\n",
                 full_url->c_str()));
  return (NACL_NO_URL != *full_url);
}


ScriptableHandle* BrowserInterfacePpapi::NewScriptableHandle(
    PortableHandle* handle) {
  return ScriptableHandlePpapi::New(handle);
}


pp::InstancePrivate* InstanceIdentifierToPPInstance(
    InstanceIdentifier instance_id) {
  return reinterpret_cast<pp::InstancePrivate*>(
      assert_cast<intptr_t>(instance_id));
}


InstanceIdentifier PPInstanceToInstanceIdentifier(
    pp::InstancePrivate* instance) {
  return assert_cast<InstanceIdentifier>(reinterpret_cast<intptr_t>(instance));
}

}  // namespace plugin
