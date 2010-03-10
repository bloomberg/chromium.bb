// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_PROXY_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_PROXY_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include "native_client/src/shared/npruntime/npcapability.h"

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

  // The NPP instance for this proxied object.
  NPP npp_;
  // The capability of the remote NPObject for which this proxy instance is
  // created.
  NPCapability capability_;

 public:
  // Creates a new instance of NPObjectProxy within the specified bridge for
  // the remote NPObject represented by the specified capability.
  NPObjectProxy(NPP npp, const NPCapability& capability);
  ~NPObjectProxy();

  // Returns true if this proxy instance is created for the remote NPObject
  // represented by the specified capability.
  bool IsMatch(const NPCapability& capability) const {
    return (capability_ == capability) ? true : false;
  }

  NPP npp() const {
    return npp_;
  }

  const NPCapability& capability() const {
    return capability_;
  }
  void set_capability(const NPCapability& capability) {
    capability_.CopyFrom(capability);
  }

  // NPClass methods. NPObjectProxy forwards these invocations to the remote
  // NPObject.
  void Deallocate();
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
  bool Enumerate(NPIdentifier* *value, uint32_t* count);
  bool Construct(const NPVariant* args, uint32_t arg_count,
                 NPVariant* result);

  // Requests NPN_SetException() to the remote NPObject.
  void SetException(const NPUTF8* message);

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
