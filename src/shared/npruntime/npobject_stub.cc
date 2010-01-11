// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface
#include "native_client/src/shared/npruntime/npobject_stub.h"

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf(stderr, "@@@ STUB ");

  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

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
  DebugPrintf("Removing a bridge.\n");
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

//
// These methods provide dispatching to the implementation of the object stubs.
//

// inputs:
// (char[]) capability
// outputs:
// (none)
NaClSrpcError NPObjectStub::Deallocate(NaClSrpcChannel* channel,
                                       NaClSrpcArg** inputs,
                                       NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("Deallocate\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  stub->DeallocateImpl();
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// outputs:
// (none)
NaClSrpcError NPObjectStub::Invalidate(NaClSrpcChannel* channel,
                                       NaClSrpcArg** inputs,
                                       NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("Invalidate\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  stub->InvalidateImpl();
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (int) name
// outputs:
// (int) success
NaClSrpcError NPObjectStub::HasMethod(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("HasMethod\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  outputs[0]->u.ival = stub->HasMethodImpl(name);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (int) name
// (char[]) args fixed portion
// (char[]) args optional portion
// (int) argument count
// outputs:
// (int) NPError
// (char[]) result fixed portion
// (char[]) result optional portion
NaClSrpcError NPObjectStub::Invoke(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("Invoke\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  RpcArg arg23(stub->npp(), inputs[2], inputs[3]);
  uint32_t arg_count = inputs[4]->u.ival;
  const NPVariant* args = arg23.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  outputs[0]->u.ival = stub->InvokeImpl(name, args, arg_count, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), outputs[1], outputs[2]);
  if (!ret12.PutVariant(&variant)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Free any allocated string in the result variant.
  if (NPERR_NO_ERROR != outputs[0]->u.ival && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (char[]) args fixed portion
// (char[]) args optional portion
// (int) argument count
// outputs:
// (int) NPError
// (char[]) result fixed portion
// (char[]) result optional portion
NaClSrpcError NPObjectStub::InvokeDefault(NaClSrpcChannel* channel,
                                          NaClSrpcArg** inputs,
                                          NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("InvokeDefault\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  RpcArg arg12(stub->npp(), inputs[1], inputs[2]);
  const uint32_t arg_count = inputs[3]->u.ival;
  const NPVariant* args = arg12.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  outputs[0]->u.ival = stub->InvokeDefaultImpl(args, arg_count, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), outputs[1], outputs[2]);
  ret12.PutVariant(&variant);
  // Free any allocated string in the result variant.
  if (NPERR_NO_ERROR != outputs[0]->u.ival && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (int) identifier
// outputs:
// (int) success
NaClSrpcError NPObjectStub::HasProperty(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("HasProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  outputs[0]->u.ival = stub->HasPropertyImpl(name);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (int) identifier
// outputs:
// (int) success
// (char[]) result fixed portion
// (char[]) result optional portion
NaClSrpcError NPObjectStub::GetProperty(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("GetProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  NPVariant variant;
  // Invoke the implementation.
  outputs[0]->u.ival = stub->GetPropertyImpl(name, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), outputs[1], outputs[2]);
  ret12.PutVariant(&variant);
  // Free any allocated string in the result variant.
  if (outputs[0]->u.ival && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (int) identifier
// (char[]) value fixed portion
// (char[]) value optional portion
// outputs:
// (int) success
NaClSrpcError NPObjectStub::SetProperty(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("SetProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  RpcArg arg23(stub->npp(), inputs[2], inputs[3]);
  const NPVariant* variant = arg23.GetVariant(true);
  outputs[0]->u.ival = stub->SetPropertyImpl(name, variant);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (int) identifier
// outputs:
// (int) success
NaClSrpcError NPObjectStub::RemoveProperty(NaClSrpcChannel* channel,
                                           NaClSrpcArg** inputs,
                                           NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("RemoveProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  outputs[0]->u.ival = stub->RemovePropertyImpl(name);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// outputs:
// (int) success
// (char[]) identifier list
// (int) identifier count
NaClSrpcError NPObjectStub::Enumerate(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("Enumerate\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  RpcArg arg1(stub->npp(), inputs[1]);
  NPIdentifier* identifiers;
  uint32_t identifier_count;
  outputs[0]->u.ival = stub->EnumerateImpl(&identifiers, &identifier_count);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (char[]) args fixed portion
// (char[]) args optional portion
// (int) argument_count
// outputs:
// (int) success
// (char[]) result fixed
// (char[]) result optional
NaClSrpcError NPObjectStub::Construct(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  DebugPrintf("Construct\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  RpcArg arg12(stub->npp(), inputs[1], inputs[2]);
  const uint32_t arg_count = inputs[3]->u.ival;
  const NPVariant* args = arg12.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  outputs[0]->u.ival = stub->ConstructImpl(args, inputs[1]->u.ival, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), outputs[1], outputs[2]);
  ret12.PutVariant(&variant);
  // Free any allocated string in the result variant.
  if (NPERR_NO_ERROR != outputs[0]->u.ival && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (char[]) capability
// (char*) message
// outputs:
// (none)
NaClSrpcError NPObjectStub::SetException(NaClSrpcChannel* channel,
                                         NaClSrpcArg** inputs,
                                         NaClSrpcArg** outputs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("SetException\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = GetByArg(&arg0);
  const NPUTF8* message = reinterpret_cast<NPUTF8*>(inputs[1]->u.sval);
  stub->SetExceptionImpl(message);
  return NACL_SRPC_RESULT_OK;
}


//
// These methods provide the implementation of the object stubs.
//

void NPObjectStub::DeallocateImpl() {
  DebugPrintf("Deallocate(%p)\n", reinterpret_cast<void*>(object_));
  // TODO(sehr): remove stub, etc.
}

void NPObjectStub::InvalidateImpl() {
  DebugPrintf("Invalidate(%p)\n", reinterpret_cast<void*>(object_));

  if (object_->_class && object_->_class->invalidate) {
    object_->_class->invalidate(object_);
    object_->referenceCount = 1;
  }
}

bool NPObjectStub::HasMethodImpl(NPIdentifier name) {
  DebugPrintf("HasMethod(%p, ", reinterpret_cast<void*>(object_));
  PrintIdent(name);
  printf(")\n");

  return NPN_HasMethod(npp_, object_, name);
}

NPError NPObjectStub::InvokeImpl(NPIdentifier name,
                                 const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* variant) {
  DebugPrintf("Invoke(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(", [");
  for (uint32_t i = 0; i < arg_count; ++i) {
    PrintVariant(args + i);
    if (i < arg_count -1) {
      printf(", ");
    }
  }
  printf("], %u)\n", static_cast<unsigned int>(arg_count));

  NPError return_value = NPN_Invoke(npp_,
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

NPError NPObjectStub::InvokeDefaultImpl(const NPVariant* args,
                                        uint32_t arg_count,
                                        NPVariant* variant) {
  DebugPrintf("InvokeDefault(%p, [", reinterpret_cast<void*>(this));
  for (uint32_t i = 0; i < arg_count; ++i) {
    PrintVariant(args + i);
    if (i < arg_count -1) {
      printf(", ");
    }
  }
  printf("], %u)\n", static_cast<unsigned int>(arg_count));

  NPError return_value = NPN_InvokeDefault(npp_,
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

bool NPObjectStub::HasPropertyImpl(NPIdentifier name) {
  DebugPrintf("HasProperty(%p, ", reinterpret_cast<void*>(object_));
  PrintIdent(name);
  printf(")\n");

  return NPN_HasProperty(npp_, object_, name);
}

bool NPObjectStub::GetPropertyImpl(NPIdentifier name, NPVariant* variant) {
  DebugPrintf("GetProperty(%p, ", reinterpret_cast<void*>(object_));
  PrintIdent(name);
  printf(")\n");

  return NPN_GetProperty(npp_, object_, name, variant);
}

bool NPObjectStub::SetPropertyImpl(NPIdentifier name,
                                   const NPVariant* variant) {
  DebugPrintf("SetProperty(%p, ", reinterpret_cast<void*>(object_));
  PrintIdent(name);
  printf(")\n");

  bool return_value = NPN_SetProperty(npp_, object_, name, variant);
  if (NPVARIANT_IS_OBJECT(*variant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(*variant);
    NPN_ReleaseObject(object);
  }
  return return_value;
}

bool NPObjectStub::RemovePropertyImpl(NPIdentifier name) {
  DebugPrintf("RemoveProperty(%p, ", reinterpret_cast<void*>(object_));
  PrintIdent(name);
  printf(")\n");

  return NPN_RemoveProperty(npp_, object_, name);
}

bool NPObjectStub::EnumerateImpl(NPIdentifier** identifiers,
                                 uint32_t* identifier_count) {
  DebugPrintf("Enumerate(%p)\n", reinterpret_cast<void*>(object_));
  return NPN_Enumerate(npp_, object_, identifiers, identifier_count);
}

bool NPObjectStub::ConstructImpl(const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* variant) {
  DebugPrintf("Construct(%p)\n", reinterpret_cast<void*>(object_));
  bool return_value = NPN_Construct(npp_, object_, args, arg_count, variant);
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NPVARIANT_IS_OBJECT(args[i])) {
      NPN_ReleaseObject(NPVARIANT_TO_OBJECT(args[i]));
    }
  }
  return return_value;
}

void NPObjectStub::SetExceptionImpl(const NPUTF8* message) {
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
      if (NPBridge::LookupBridge(npp)->peer_pid() == proxy->capability().pid) {
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
      cap->pid = GETPID();
      cap->object = object;
      return 1;
    }
    // Create a new stub for the object.
    stub = new(std::nothrow) NPObjectStub(npp, object);
  }
  // Create a capability for NULL or the newly created stub.
  cap->pid = GETPID();
  if (NULL == stub) {
    // If the stub was null, either the object was null or new failed.
    // In either case, we don't have a stub to give a capability to.
    cap->object = NULL;
    return 0;
  }
  // Insert the newly created stub into the mapping for future reuse.
  assert(NULL != stub_map);
  (*stub_map)[object] = stub;
  cap->object = object;
  return 1;
}

NPObjectStub* NPObjectStub::GetByObject(NPObject* object) {
  assert(object);
  assert(NULL != stub_map);
  // Find the object in the stub table.
  std::map<NPObject*, NPObjectStub*>::iterator i = stub_map->find(object);
  if (stub_map->end() == i) {
    // There is no capability for the specified object in the table.
    return NULL;
  }
  return (*i).second;
}

NPObjectStub* NPObjectStub::GetByCapability(const NPCapability& capability) {
  if (GETPID() != capability.pid) {
    // Only capabilities to objects in this process have stubs in the table.
    return NULL;
  }
  return GetByObject(capability.object);
}

NPObjectStub* NPObjectStub::GetByArg(RpcArg* arg) {
  NPCapability* capability = arg->GetCapability();
  return GetByCapability(*capability);
}

}  // namespace nacl
