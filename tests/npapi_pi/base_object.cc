/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/tests/npapi_pi/plugin.h"

namespace {

void Deallocate(NPObject* object) {
  delete static_cast<BaseObject*>(object);
}

void Invalidate(NPObject* object) {
  return static_cast<BaseObject*>(object)->Invalidate();
}

bool HasMethod(NPObject* object, NPIdentifier name) {
  return static_cast<BaseObject*>(object)->HasMethod(name);
}

bool Invoke(NPObject* object, NPIdentifier name,
            const NPVariant* args, uint32_t arg_count,
            NPVariant* result) {
  return static_cast<BaseObject*>(object)->Invoke(
      name, args, arg_count, result);
}

bool InvokeDefault(NPObject* object, const NPVariant* args, uint32_t arg_count,
                   NPVariant* result) {
  return static_cast<BaseObject*>(object)->InvokeDefault(
      args, arg_count, result);
}

bool HasProperty(NPObject* object, NPIdentifier name) {
  return static_cast<BaseObject*>(object)->HasProperty(name);
}

bool GetProperty(NPObject* object, NPIdentifier name, NPVariant* result) {
  return static_cast<BaseObject*>(object)->GetProperty(name, result);
}

bool SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value) {
  return static_cast<BaseObject*>(object)->SetProperty(name, value);
}

bool RemoveProperty(NPObject* object, NPIdentifier name) {
  return static_cast<BaseObject*>(object)->RemoveProperty(name);
}

NPObject* AllocateScriptablePluginObject(NPP npp, NPClass* npclass) {
  return new ScriptablePluginObject(npp);
}

}  // namespace

NPClass ScriptablePluginObject::np_class = {
  NP_CLASS_STRUCT_VERSION,
  ::AllocateScriptablePluginObject,
  ::Deallocate,
  ::Invalidate,
  ::HasMethod,
  ::Invoke,
  ::InvokeDefault,
  ::HasProperty,
  ::GetProperty,
  ::SetProperty,
  ::RemoveProperty
};
