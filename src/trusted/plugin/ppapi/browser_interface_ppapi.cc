/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

namespace plugin {

uintptr_t BrowserInterfacePpapi::StringToIdentifier(const nacl::string& str) {
  UNREFERENCED_PARAMETER(str);
  NACL_UNIMPLEMENTED();
  return 0;
}


nacl::string BrowserInterfacePpapi::IdentifierToString(uintptr_t ident) {
  UNREFERENCED_PARAMETER(ident);
  NACL_UNIMPLEMENTED();
  return "";
}


bool BrowserInterfacePpapi::Alert(InstanceIdentifier instance_id,
                                  const nacl::string& text) {
  UNREFERENCED_PARAMETER(instance_id);
  UNREFERENCED_PARAMETER(text);
  NACL_UNIMPLEMENTED();
  return false;
}


bool BrowserInterfacePpapi::MightBeElfExecutable(const nacl::string& filename,
                                                 nacl::string* error) {
  UNREFERENCED_PARAMETER(filename);
  UNREFERENCED_PARAMETER(error);
  NACL_UNIMPLEMENTED();
  return false;
}


bool BrowserInterfacePpapi::MightBeElfExecutable(const char* buffer,
                                                 size_t size,
                                                 nacl::string* error) {
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(error);
  NACL_UNIMPLEMENTED();
  return false;
}


bool BrowserInterfacePpapi::EvalString(InstanceIdentifier instance_id,
                                       const nacl::string& expression) {
  UNREFERENCED_PARAMETER(instance_id);
  UNREFERENCED_PARAMETER(expression);
  NACL_UNIMPLEMENTED();
  return false;
}


bool BrowserInterfacePpapi::GetOrigin(InstanceIdentifier instance_id,
                                      nacl::string* origin) {
  UNREFERENCED_PARAMETER(instance_id);
  UNREFERENCED_PARAMETER(origin);
  NACL_UNIMPLEMENTED();
  return false;
}


ScriptableHandle* BrowserInterfacePpapi::NewScriptableHandle(
    PortableHandle* handle) {
  UNREFERENCED_PARAMETER(handle);
  NACL_UNIMPLEMENTED();
  return ScriptableHandlePpapi::New(handle);
}

}  // namespace plugin
