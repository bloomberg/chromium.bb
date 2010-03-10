// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_

#include <map>
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"

namespace nacl {

class NPBridge;
class RpcArg;

// An NPObjectStub wraps an NPObject and converts IMC messages from
// NPObjectProxy to calls to the object.
class NPObjectStub {
 public:
  // Returns true if this stub instance is created for the local NPObject
  // represented by the specified capability.
  bool IsMatch(const NPCapability& capability) const {
    return object_ == capability.object();
  }

  NPP npp() const {
    return npp_;
  }

  NPObject* object() const {
    return object_;
  }

  // An NPBridge (NPModule or NPNavigator) registers that it will be creating
  // or using stubs.  When no NPBridges are live, the stubs can be released.
  // TODO(sehr): there are still some lifetime and reentrancy issues that
  // need to be worked on here.
  static bool AddBridge();
  static bool RemoveBridge(bool release_objects);

  // This implementation is an exception because it is also called from
  // npn_gate.cc
  void SetException(const NPUTF8* message);

  static NPObjectStub* GetStub(char* capability_bytes,
                               nacl_abi_size_t capability_length);

  // Creates an NPObject stub for the specified object. On success,
  // CreateStub() returns 1, and NPCapability for the stub is filled in.
  // CreateStub() returns 0 on failure.
  static int CreateStub(NPP npp, NPObject* object, NPCapability* cap);

  // Returns the NPObject stub that corresponds to the specified object.
  // GetByObject() returns NULL if no correspoing stub is found.
  static NPObjectStub* GetByObject(NPObject* object);

  // Returns the NPObject stub that corresponds to the specified capability.
  // GetByCapability() returns NULL if no correspoing stub is found.
  static NPObjectStub* GetByCapability(const NPCapability* capability);

  // NPClass methods. NPObjectStub forwards these RPC requests received from
  // the remote NPObjectProxy object to object_;
  void Deallocate();
  void Invalidate();
  bool HasMethod(NPIdentifier name);
  bool Invoke(NPIdentifier name,
              const NPVariant* args,
              uint32_t arg_count,
              NPVariant* result);
  bool InvokeDefault(const NPVariant* args,
                     uint32_t arg_count,
                     NPVariant* result);
  bool HasProperty(NPIdentifier name);
  bool GetProperty(NPIdentifier name, NPVariant* result);
  bool SetProperty(NPIdentifier name, const NPVariant* variant);
  bool RemoveProperty(NPIdentifier name);
  bool Enumerate(NPIdentifier** identifiers,
                 uint32_t* identifier_count);
  bool Construct(const NPVariant* args,
                 uint32_t arg_count,
                 NPVariant* result);

 private:
  // Creates a new instance of NPObjectStub with the specified NPP for
  // the specified local NPObject.
  NPObjectStub(NPP npp, NPObject* object);
  ~NPObjectStub();

  // The mapping from NPObject to NPObjectStub.
  // Thread safe because it is only accessed from the NPAPI main thread.
  // TODO(sehr): this might be better as a hash_map.
  static std::map<NPObject*, NPObjectStub*> *stub_map;
  // Reference count of the number of NPBridges that have been created.
  // Thread safe because it is only accessed from the NPAPI main thread.
  static int32_t bridge_ref_count;

  // The NPP instance to which this stub belongs.
  NPP npp_;
  // The NPObject for which this stub instance is created.
  NPObject* object_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_
