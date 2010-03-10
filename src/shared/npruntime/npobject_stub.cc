// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface
#include "native_client/src/shared/npruntime/npobject_stub.h"

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/structure_translations.h"

namespace nacl {

std::map<NPObject*, NPObjectStub*> *NPObjectStub::stub_map = NULL;
int32_t NPObjectStub::bridge_ref_count = 0;

bool NPObjectStub::AddBridge() {
  if (NULL == stub_map) {
    stub_map = new(std::nothrow) std::map<NPObject*, NPObjectStub*>;
  }
  if (NULL == stub_map) {
    return false;
  }
  ++bridge_ref_count;
  if (0 > bridge_ref_count) {
    // Signed integer overflow.
    delete stub_map;
    stub_map = NULL;
    return false;
  }
  return true;
}

bool NPObjectStub::RemoveBridge(bool release_objects) {
  // Sanity check to make sure that we can't remove more bridges than we
  // added.
  if (0 >= bridge_ref_count) {
    DebugPrintf("ERROR: attempted to remove more bridges than were added.\n");
    return false;
  }
  --bridge_ref_count;
  if (0 == bridge_ref_count) {
    if (NULL == stub_map) {
      DebugPrintf("ERROR: stub_map was null at zero ref count\n");
      return false;
    }
    // Release the NPObjectStubs contained in the map.
    std::map<NPObject*, NPObjectStub*>::iterator i;
    // Save stub_map to avoid possible reentrancy issues.
    std::map<NPObject*, NPObjectStub*>* tmp_stub_map = stub_map;
    stub_map = NULL;
    for (i = tmp_stub_map->begin(); i != tmp_stub_map->end(); ++i) {
      NPObjectStub* stub = i->second;
      if (release_objects) {
        // release_objects needs to be true only if we are certain that
        // NPN_Invalidate hasn't been called on the object.  If so, we can
        // safely decrement the refcount on the object the stub referred to.
        NPN_ReleaseObject(stub->object());
      }
      // Delete the stub.
      delete stub;
    }
    // Delete the entire map.
    delete tmp_stub_map;
  }
  return true;
}

NPObjectStub::NPObjectStub(NPP npp, NPObject* object)
    : npp_(npp),
      object_(object) {
  NPN_RetainObject(object_);
}

NPObjectStub::~NPObjectStub() {
}

NPObjectStub* NPObjectStub::GetStub(char* capability_bytes,
                                    nacl_abi_size_t capability_length) {
  NPCapability* capability =
      WireFormatToNPCapability(capability_bytes, capability_length, NULL);
  if (NULL == capability) {
    return NULL;
  }
  NPObjectStub* stub = GetByCapability(capability);
  delete capability;
  return stub;
}

//
// These methods provide the implementation of the object stubs.
//

void NPObjectStub::Deallocate() {
  DebugPrintf("Deallocate(%p)\n", reinterpret_cast<void*>(object_));
  // TODO(sehr): remove stub, etc.
}

void NPObjectStub::Invalidate() {
  DebugPrintf("Invalidate(%p)\n", reinterpret_cast<void*>(object_));

  if (object_->_class && object_->_class->invalidate) {
    object_->_class->invalidate(object_);
    object_->referenceCount = 1;
  }
}

bool NPObjectStub::HasMethod(NPIdentifier name) {
  DebugPrintf("HasMethod(%p, %s)\n",
              reinterpret_cast<void*>(object_),
              FormatNPIdentifier(name));

  return NPN_HasMethod(npp_, object_, name);
}

bool NPObjectStub::Invoke(NPIdentifier name,
                              const NPVariant* args,
                              uint32_t arg_count,
                              NPVariant* variant) {
  DebugPrintf("Invoke(%p, %s, %s, %u)\n",
              reinterpret_cast<void*>(object_),
              FormatNPIdentifier(name),
              FormatNPVariantVector(args, arg_count),
              static_cast<unsigned int>(arg_count));

  bool return_value = NPN_Invoke(npp_,
                                 object_,
                                 name,
                                 args,
                                 arg_count,
                                 variant);
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NPVARIANT_IS_OBJECT(args[i])) {
      NPN_ReleaseObject(NPVARIANT_TO_OBJECT(args[i]));
    }
  }
  return return_value;
}

bool NPObjectStub::InvokeDefault(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* variant) {
  DebugPrintf("InvokeDefault(%p, %s, %u)\n",
              reinterpret_cast<void*>(object_),
              FormatNPVariantVector(args, arg_count),
              static_cast<unsigned int>(arg_count));

  bool return_value = NPN_InvokeDefault(npp_,
                                        object_,
                                        args,
                                        arg_count,
                                        variant);
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NPVARIANT_IS_OBJECT(args[i])) {
      NPN_ReleaseObject(NPVARIANT_TO_OBJECT(args[i]));
    }
  }
  return return_value;
}

bool NPObjectStub::HasProperty(NPIdentifier name) {
  DebugPrintf("HasProperty(%p, %s)\n",
              reinterpret_cast<void*>(object_),
              FormatNPIdentifier(name));

  return NPN_HasProperty(npp_, object_, name);
}

bool NPObjectStub::GetProperty(NPIdentifier name, NPVariant* variant) {
  DebugPrintf("GetProperty(%p, %s)\n",
              reinterpret_cast<void*>(object_),
              FormatNPIdentifier(name));

  return NPN_GetProperty(npp_, object_, name, variant);
}

bool NPObjectStub::SetProperty(NPIdentifier name,
                               const NPVariant* variant) {
  DebugPrintf("SetProperty(%p, %s, %s)\n",
              reinterpret_cast<void*>(object_),
              FormatNPIdentifier(name),
              FormatNPVariant(variant));

  bool return_value = NPN_SetProperty(npp_, object_, name, variant);
  if (NPVARIANT_IS_OBJECT(*variant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(*variant);
    NPN_ReleaseObject(object);
  }
  return return_value;
}

bool NPObjectStub::RemoveProperty(NPIdentifier name) {
  DebugPrintf("RemoveProperty(%p, %s)\n",
              reinterpret_cast<void*>(object_),
              FormatNPIdentifier(name));

  return NPN_RemoveProperty(npp_, object_, name);
}

bool NPObjectStub::Enumerate(NPIdentifier** identifiers,
                             uint32_t* identifier_count) {
  DebugPrintf("Enumerate(%p)\n", reinterpret_cast<void*>(object_));

  return NPN_Enumerate(npp_, object_, identifiers, identifier_count);
}

bool NPObjectStub::Construct(const NPVariant* args,
                             uint32_t arg_count,
                             NPVariant* variant) {
  DebugPrintf("Construct(%p, %s, %u)\n",
              reinterpret_cast<void*>(object_),
              FormatNPVariantVector(args, arg_count),
              static_cast<unsigned int>(arg_count));

  bool return_value = NPN_Construct(npp_, object_, args, arg_count, variant);
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NPVARIANT_IS_OBJECT(args[i])) {
      NPN_ReleaseObject(NPVARIANT_TO_OBJECT(args[i]));
    }
  }
  return return_value;
}

void NPObjectStub::SetException(const NPUTF8* message) {
  DebugPrintf("SetException(%p, %s)\n",
              reinterpret_cast<void*>(object_),
              message);
  NPN_SetException(object_, message);
}

int NPObjectStub::CreateStub(NPP npp, NPObject* object, NPCapability* cap) {
  NPObjectStub* stub = NULL;
  if (NULL != object) {
    if (NPObjectProxy::IsInstance(object)) {
      // The specified object is a proxy.
      NPObjectProxy* proxy = static_cast<NPObjectProxy*>(object);
      if (NPBridge::LookupBridge(npp)->peer_pid() ==
          proxy->capability().pid()) {
        // The proxy is for an object in the peer process for this npp.
        // Accesses to the object should be done by the existing capability.
        *cap = proxy->capability();
        return 0;
      }
      // If control reaches this point, the object is a proxy, but for an
      // object in a process (P2) different than the peer (P1) of this npp.  We
      // therefore need to create a local stub for the proxy to P2, so that P1
      // can talk to the local stub and the proxy can the talk to P2.
    }
    // Look up the object in the stub mapping.
    if (NULL != GetByObject(object)) {
      // There is already a stub for this object in the mapping.
      cap->set_pid(GETPID());
      cap->set_object(object);
      return 1;
    }
    // Create a new stub for the object.
    stub = new(std::nothrow) NPObjectStub(npp, object);
  }
  // Create a capability for NULL or the newly created stub.
  cap->set_pid(GETPID());
  if (NULL == stub) {
    // If the stub was null, either the object was null or new failed.
    // In either case, we don't have a stub to give a capability to.
    cap->set_object(NULL);
    return 0;
  }
  // Insert the newly created stub into the mapping for future reuse.
  assert(NULL != stub_map);
  (*stub_map)[object] = stub;
  cap->set_object(object);
  return 1;
}

NPObjectStub* NPObjectStub::GetByObject(NPObject* object) {
  if (NULL == object || NULL == stub_map) {
    return NULL;
  }
  // Find the object in the stub table.
  std::map<NPObject*, NPObjectStub*>::iterator i = stub_map->find(object);
  if (stub_map->end() == i) {
    // There is no capability for the specified object in the table.
    return NULL;
  }
  return (*i).second;
}

NPObjectStub* NPObjectStub::GetByCapability(const NPCapability* capability) {
  if (GETPID() != capability->pid()) {
    // Only capabilities to objects in this process have stubs in the table.
    return NULL;
  }
  return GetByObject(capability->object());
}

}  // namespace nacl
