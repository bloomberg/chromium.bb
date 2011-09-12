// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include "native_client/src/trusted/plugin/browser_interface.h"

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/elf.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/private/var_private.h"

using nacl::assert_cast;

namespace plugin {

uintptr_t BrowserInterface::StringToIdentifier(const nacl::string& str) {
  StringToIdentifierMap::iterator iter = string_to_identifier_map_.find(str);
  if (iter == string_to_identifier_map_.end()) {
    uintptr_t id = next_identifier++;
    string_to_identifier_map_.insert(make_pair(str, id));
    identifier_to_string_map_.insert(make_pair(id, str));
    return id;
  }
  return string_to_identifier_map_[str];
}


nacl::string BrowserInterface::IdentifierToString(uintptr_t ident) {
  assert(identifier_to_string_map_.find(ident) !=
         identifier_to_string_map_.end());
  return identifier_to_string_map_[ident];
}


void BrowserInterface::AddToConsole(pp::InstancePrivate* instance,
                                    const nacl::string& text) {
  pp::Module* module = pp::Module::Get();
  const PPB_Var* var_interface =
      static_cast<const struct PPB_Var*>(
          module->GetBrowserInterface(PPB_VAR_INTERFACE));
  nacl::string prefix_string("NativeClient");
  PP_Var prefix =
      var_interface->VarFromUtf8(module->pp_module(),
                                 prefix_string.c_str(),
                                 static_cast<uint32_t>(prefix_string.size()));
  PP_Var str = var_interface->VarFromUtf8(module->pp_module(),
                                          text.c_str(),
                                          static_cast<uint32_t>(text.size()));
  const PPB_Console_Dev* console_interface =
      static_cast<const struct PPB_Console_Dev*>(
          module->GetBrowserInterface(PPB_CONSOLE_DEV_INTERFACE));
  console_interface->LogWithSource(instance->pp_instance(),
                                   PP_LOGLEVEL_LOG,
                                   prefix,
                                   str);
  var_interface->Release(prefix);
  var_interface->Release(str);
}

}  // namespace plugin
