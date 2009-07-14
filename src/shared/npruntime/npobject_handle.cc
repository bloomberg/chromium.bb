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


#include "native_client/src/shared/npruntime/npobject_handle.h"

#include "native_client/src/shared/npruntime/npbridge.h"

namespace {

NPObject* Alloc(NPP npp, NPClass* aClass) {
  return nacl::NPHandleObject::GetLastAllocated();
}

void Deallocate(NPObject* object) {
  delete static_cast<nacl::NPHandleObject*>(object);
}

void Invalidate(NPObject* object) {
  return static_cast<nacl::NPHandleObject*>(object)->Invalidate();
}

bool HasMethod(NPObject* object, NPIdentifier name) {
  return static_cast<nacl::NPHandleObject*>(object)->HasMethod(name);
}

bool Invoke(NPObject* object, NPIdentifier name,
            const NPVariant* args, uint32_t arg_count,
            NPVariant* result) {
  return static_cast<nacl::NPHandleObject*>(object)->Invoke(
      name, args, arg_count, result);
}

bool InvokeDefault(NPObject* object, const NPVariant* args, uint32_t arg_count,
                   NPVariant* result) {
  return static_cast<nacl::NPHandleObject*>(object)->InvokeDefault(
      args, arg_count, result);
}

bool HasProperty(NPObject* object, NPIdentifier name) {
  return static_cast<nacl::NPHandleObject*>(object)->HasProperty(name);
}

bool GetProperty(NPObject* object, NPIdentifier name, NPVariant* result) {
  return static_cast<nacl::NPHandleObject*>(object)->GetProperty(name, result);
}

bool SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value) {
  return static_cast<nacl::NPHandleObject*>(object)->SetProperty(name, value);
}

bool RemoveProperty(NPObject* object, NPIdentifier name) {
  return static_cast<nacl::NPHandleObject*>(object)->RemoveProperty(name);
}

NPIdentifier id_handle = NULL;

}  // namespace

namespace nacl {

NPClass NPHandleObject::np_class = {
  NP_CLASS_STRUCT_VERSION,
  ::Alloc,
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

NPObject* NPHandleObject::last_allocated;

NPHandleObject::NPHandleObject(NPBridge* bridge, HtpHandle handle) {
  if (id_handle == NULL) {
    id_handle = NPN_GetStringIdentifier("handle");
  }
  handle_ = handle;
  last_allocated = this;
  NPN_CreateObject(bridge->npp(), &np_class);
}

NPHandleObject::~NPHandleObject() {
  Close(handle_);
}

void NPHandleObject::Invalidate() {
}

bool NPHandleObject::HasMethod(NPIdentifier name) {
  return false;
}

bool NPHandleObject::Invoke(NPIdentifier name,
                           const NPVariant* args, uint32_t arg_count,
                           NPVariant* variant) {
  return false;
}

bool NPHandleObject::InvokeDefault(const NPVariant* args, uint32_t arg_count,
                                  NPVariant* variant) {
  return true;
}

bool NPHandleObject::HasProperty(NPIdentifier name) {
  return name == id_handle;
}

bool NPHandleObject::GetProperty(NPIdentifier name, NPVariant* variant) {
  static const int kHandleStringLength = 19;

  if (name == id_handle && variant) {
    if (char* hex = static_cast<char*>(NPN_MemAlloc(kHandleStringLength))) {
#if NACL_WINDOWS
      _snprintf_s(hex, kHandleStringLength, kHandleStringLength,
                  "0x%p", (void*) handle_);
#else
      snprintf(hex, kHandleStringLength, "%p", (void*) handle_);
#endif

      STRINGZ_TO_NPVARIANT(hex, *variant);
      return true;
    }
  }
  return false;
}

bool NPHandleObject::SetProperty(NPIdentifier name, const NPVariant* value) {
  return false;
}

bool NPHandleObject::RemoveProperty(NPIdentifier name) {
  return false;
}

}  // namespace nacl
