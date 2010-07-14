/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"

#include "native_client/src/include/nacl_macros.h"

namespace plugin {

ScriptableHandlePpapi* ScriptableHandlePpapi::New(PortableHandle* handle) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::New: handle=%p\n",
                 static_cast<void*>(handle)));
  if (handle == NULL) {
    return NULL;
  }
  ScriptableHandlePpapi* scriptable_handle =
      new(std::nothrow) ScriptableHandlePpapi(handle);
  if (scriptable_handle == NULL) {
    return NULL;
  }
  scriptable_handle->set_handle(handle);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::New: scriptable_handle=%p - done\n",
                 static_cast<void*>(scriptable_handle)));
  return scriptable_handle;
}


ScriptableHandle* ScriptableHandlePpapi::AddRef() {
  NACL_UNIMPLEMENTED();
  return NULL;
}


void ScriptableHandlePpapi::Unref() {
  NACL_UNIMPLEMENTED();
}


ScriptableHandlePpapi::ScriptableHandlePpapi(PortableHandle* handle)
  : ScriptableHandle(handle) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::ScriptableHandlePpapi: this=%p, "
                 "handle=%p\n",
                 static_cast<void*>(this), static_cast<void*>(handle)));
}


ScriptableHandlePpapi::~ScriptableHandlePpapi() {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::~ScriptableHandlePpapi: this=%p\n",
                 static_cast<void*>(this)));
}

}  // namespace plugin
