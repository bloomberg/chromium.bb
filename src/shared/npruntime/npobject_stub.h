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


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_

// TODO(sehr): prune the included files.
#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/nprpc.h"

namespace nacl {

class NPBridge;
class RpcArg;

// An NPObjectStub wraps an NPObject and converts IMC messages from
// NPObjectProxy to calls to the object.
class NPObjectStub {
  // The NPBridge that manages this stub instance.
  NPBridge* bridge_;
  // The NPObject for which this stub instance is created.
  NPObject* object_;

 public:
  // Creates a new instance of NPObjectStub within the specified bridge for
  // the specified local NPObject.
  NPObjectStub(NPBridge* bridge, NPObject* object);
  ~NPObjectStub();

  // Returns true if this stub instance is created for the local NPObject
  // represented by the specified capability.
  bool IsMatch(const NPCapability& capability) const {
    return object_ == capability.object;
  }

  NPObject* object() const {
    return object_;
  }

  // Dispatches the request message received from the remote NPObjectProxy
  // object.
  int Dispatch(RpcHeader* request, int len);

  // NPClass methods. NPObjectStub forwards these RPC requests received from
  // the remote NPObjectProxy object to object_;
  bool Invalidate(RpcArg* arg);
  bool HasMethod(RpcArg* arg);
  bool Invoke(uint32_t arg_count, RpcArg* arg, NPVariant* result);
  bool InvokeDefault(uint32_t arg_count, RpcArg* arg, NPVariant* result);
  bool HasProperty(RpcArg* arg);
  bool GetProperty(RpcArg* arg, NPVariant* result);
  bool SetProperty(RpcArg* arg);
  bool RemoveProperty(RpcArg* arg);
  bool Enumeration(RpcArg* arg);
  bool Construct(uint32_t arg_count, RpcArg* arg, NPVariant* result);

  // Processes NPN_SetException() request from the remote proxy object.
  void SetException(const NPUTF8* message);
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_
