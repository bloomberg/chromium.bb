/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
