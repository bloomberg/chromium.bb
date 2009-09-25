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

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_PROXY_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_PROXY_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/nprpc.h"

namespace nacl {

class NPBridge;

// An NPObjectProxy represents a remote NPObject. All calls to it are sent
// across an IMC channel through NPBridge. The NPObjectStub on the other
// side translates the received RPC messages into calls to the actual NPObject,
// and sends back the RPC response messages.
//
// Note reference counting of NPObjectProxy is processed through NPObject
// methods.
class NPObjectProxy : public NPObject {
  // The NPClass object for NPObjectProxy.
  static NPClass np_class;
  // The pointer to the NPObjectProxy instance that is created lastly with the
  // constructor.
  static NPObject* last_allocated;

  // The NPBridge that manages this proxy instance.
  NPBridge* bridge_;
  // The capability of the remote NPObject for which this proxy instance is
  // created.
  NPCapability capability_;

 public:
  // Creates a new instance of NPObjectProxy within the specified bridge for
  // the remote NPObject represented by the specified capability.
  NPObjectProxy(NPBridge* bridge, const NPCapability& capability);
  ~NPObjectProxy();

  // Returns true if this proxy instance is created for the remote NPObject
  // represented by the specified capability.
  bool IsMatch(const NPCapability& capability) const {
    return (capability_ == capability) ? true : false;
  }

  const NPCapability& capability() const {
    return capability_;
  }
  void set_capability(const NPCapability& capability) {
    capability_.CopyFrom(capability);
  }

  // NPClass methods. NPObjectProxy forwards these invocations to the remote
  // NPObject.
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
  bool Enumeration(NPIdentifier* *value, uint32_t* count);
  bool Construct(const NPVariant* args, uint32_t arg_count,
                 NPVariant* result);

  // Requests NPN_SetException() to the remote NPObject.
  void SetException(const NPUTF8* message);

  // Processes NPN_SetException() request from the remote NPObject.
  int SetException(RpcHeader* request, int len);

  // Detaches this instance from the bridge. Note Invalidate() can be called
  // after NPP_Destroy() is called and the bridge is deleted.
  void Detach() {
    bridge_ = NULL;
  }

  // Returns the pointer to the NPObjectProxy instance that is created lastly
  // with the constructor.
  static NPObject* GetLastAllocated() {
    return last_allocated;
  }

  // Returns true if the specified object is of type NPObjectProxy.
  static bool IsInstance(NPObject* object) {
    return (object->_class == &np_class) ? true : false;
  }
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_PROXY_H_
