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


#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_HANDLE_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_HANDLE_H_

#include "native_client/src/shared/imc/nacl_htp.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl {

class NPBridge;

// NPHandleObject encapsulates a NaCl handle as an NPObject.
class NPHandleObject : public NPObject {
  // The NPBridge that manages this handle object instance.
  static NPClass np_class;
  // The NPObject for which this handle object instance is created.
  static NPObject* last_allocated;

  // The encapsulated handle object.
  HtpHandle handle_;

 public:
  // Creates a new instance of NPHandleObject within the specified bridge for
  // the specified handle.
  NPHandleObject(NPBridge* bridge, HtpHandle handle);
  ~NPHandleObject();

  HtpHandle handle() const {
    return handle_;
  }

  // NPClass methods.
  void Invalidate();
  bool HasMethod(NPIdentifier name);
  bool Invoke(NPIdentifier name, const NPVariant* args, uint32_t arg_count,
              NPVariant* result);
  bool InvokeDefault(const NPVariant* args, uint32_t arg_count,
                     NPVariant* result);
  bool HasProperty(NPIdentifier name);
  bool GetProperty(NPIdentifier name, NPVariant* result);
  bool SetProperty(NPIdentifier name, const NPVariant* value);
  bool RemoveProperty(NPIdentifier name);

  // Returns the pointer to the NPHandleObject instance that is created lastly
  // with the constructor.
  static NPObject* GetLastAllocated() {
    return last_allocated;
  }

  // Returns true if the specified object is of type NPHandleObject.
  static bool IsInstance(NPObject* object) {
    return (object->_class == &np_class) ? true : false;
  }
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_HANDLE_H_
