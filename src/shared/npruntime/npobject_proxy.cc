// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface
// Ojbect proxying allows scripting objects across two different processes.
// The "proxy" side is in the process scripting the object.
// The "stub" side is in the process implementing the object.

#include "native_client/src/shared/npruntime/npobject_proxy.h"

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/npruntime/structure_translations.h"

using nacl::NPBridge;
using nacl::NPObjectProxy;
using nacl::NPVariantsToWireFormat;
using nacl::WireFormatToNPVariants;

namespace {

// These methods populate the NPClass for an object proxy:
// Alloc, Deallocate, Invalidate, HasMethod, Invoke, InvokeDefault, HasProperty,
// GetProperty, SetProperty, RemoveProperty, Enumerate, and Construct.
// They simply extract the proxy address and invoke the corresponding
// function on that object.
NPObject* Alloc(NPP npp, NPClass* aClass) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(aClass);

  return NPObjectProxy::GetLastAllocated();
}

void Deallocate(NPObject* object) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  nacl::DebugPrintf("Deallocate(%p)\n", reinterpret_cast<void*>(object));
  delete npobj;
}

// Invalidate is called after NPP_Destroy...
void Invalidate(NPObject* object) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->Invalidate();
}

bool HasMethod(NPObject* object, NPIdentifier name) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->HasMethod(name);
}

bool Invoke(NPObject* object,
            NPIdentifier name,
            const NPVariant* args,
            uint32_t arg_count,
            NPVariant* result) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->Invoke(name, args, arg_count, result);
}

bool InvokeDefault(NPObject* object,
                   const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->InvokeDefault(args, arg_count, result);
}

bool HasProperty(NPObject* object, NPIdentifier name) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->HasProperty(name);
}

bool GetProperty(NPObject* object, NPIdentifier name, NPVariant* result) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->GetProperty(name, result);
}

bool SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->SetProperty(name, value);
}

bool RemoveProperty(NPObject* object, NPIdentifier name) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->RemoveProperty(name);
}

bool Enumerate(NPObject* object, NPIdentifier* *value, uint32_t* count) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->Enumerate(value, count);
}

bool Construct(NPObject* object,
               const NPVariant* args,
               uint32_t arg_count,
               NPVariant* result) {
  NPObjectProxy* npobj = static_cast<NPObjectProxy*>(object);
  return npobj->Construct(args, arg_count, result);
}

}  // namespace

namespace nacl {

#if 1 <= NP_VERSION_MAJOR || 19 <= NP_VERSION_MINOR

NPClass NPObjectProxy::np_class = {
  NP_CLASS_STRUCT_VERSION_CTOR,
  ::Alloc,
  ::Deallocate,
  ::Invalidate,
  ::HasMethod,
  ::Invoke,
  ::InvokeDefault,
  ::HasProperty,
  ::GetProperty,
  ::SetProperty,
  ::RemoveProperty,
  ::Enumerate,
  ::Construct
};

#else   // 1 <= NP_VERSION_MAJOR || 19 <= NP_VERSION_MINOR

NPClass NPObjectProxy::np_class = {
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

#endif  // 1 <= NP_VERSION_MAJOR || 19 <= NP_VERSION_MINOR

NPObject* NPObjectProxy::last_allocated;

NPObjectProxy::NPObjectProxy(NPP npp, const NPCapability& capability)
    : npp_(npp) {
  DebugPrintf("NPObjectProxy(%p)\n", reinterpret_cast<void*>(this));
  capability_.CopyFrom(capability);
  last_allocated = this;
  NPN_CreateObject(npp_, &np_class);
  last_allocated = NULL;
}

NPObjectProxy::~NPObjectProxy() {
  DebugPrintf("~NPObjectProxy(%p)\n", reinterpret_cast<void*>(this));
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  Deallocate();
  bridge->RemoveProxy(this);
}

void NPObjectProxy::Deallocate() {
  DebugPrintf("Deallocate(%p)\n", reinterpret_cast<void*>(this));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  // Perform the RPC.
  NPObjectStubRpcClient::NPN_Deallocate(bridge->channel(),
                                        NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr());
}

void NPObjectProxy::Invalidate() {
  DebugPrintf("Invalidate(%p)\n", reinterpret_cast<void*>(this));

  // Note Invalidate() can be called after NPP_Destroy() is called.
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  // Perform the RPC.
  NPObjectStubRpcClient::NPN_Invalidate(bridge->channel(),
                                        NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr());
}

bool NPObjectProxy::HasMethod(NPIdentifier id) {
  DebugPrintf("HasMethod(%p, %s)\n",
              reinterpret_cast<void*>(this),
              FormatNPIdentifier(id));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t success;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_HasMethod(bridge->channel(),
                                           NPPToWireFormat(npp_),
                                           capability_.size(),
                                           capability_.char_addr(),
                                           NPIdentifierToWireFormat(id),
                                           &success);
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  return success != 0;
}

bool NPObjectProxy::Invoke(NPIdentifier id,
                           const NPVariant* args,
                           uint32_t arg_count,
                           NPVariant* variant) {
  DebugPrintf("Invoke(%p, %s, %s, %u)\n",
              reinterpret_cast<void*>(this),
              FormatNPIdentifier(id),
              FormatNPVariantVector(args, arg_count),
              static_cast<unsigned int>(arg_count));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  // Serialize the argument vector.
  nacl_abi_size_t args_length = kNaClAbiSizeTMax;
  char *args_bytes =
      NPVariantsToWireFormat(npp_, args, arg_count, NULL, &args_length);
  if ((NULL == args_bytes || 0 == args_length) && 0 != arg_count) {
    return false;
  }
  int32_t success;
  // TODO(sehr): return space should really be allocated by the invoked method.
  char ret_bytes[kNPVariantSizeMax];
  nacl_abi_size_t ret_length =
      static_cast<nacl_abi_size_t>(sizeof(ret_bytes));
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_Invoke(bridge->channel(),
                                        NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr(),
                                        NPIdentifierToWireFormat(id),
                                        args_length,
                                        args_bytes,
                                        static_cast<int32_t>(arg_count),
                                        &success,
                                        &ret_length,
                                        ret_bytes);
  // Free the serialized args.
  delete args_bytes;
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  // If the RPC was successful, get the result.
  if (success &&
      WireFormatToNPVariants(npp_, ret_bytes, ret_length, 1, variant)) {
    DebugPrintf("Invoke(%p, %s) succeeded: %s\n",
                reinterpret_cast<void*>(this),
                FormatNPIdentifier(id),
                FormatNPVariant(variant));
    return true;
  }
  DebugPrintf("Invoke(%p, %s) failed.\n",
              reinterpret_cast<void*>(this),
              FormatNPIdentifier(id));
  return false;
}

bool NPObjectProxy::InvokeDefault(const NPVariant* args,
                                  uint32_t arg_count,
                                  NPVariant* variant) {
  DebugPrintf("InvokeDefault(%p, %s, %u)\n",
              reinterpret_cast<void*>(this),
              FormatNPVariantVector(args, arg_count),
              static_cast<unsigned int>(arg_count));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  // Serialize the argument vector.
  nacl_abi_size_t args_length = kNaClAbiSizeTMax;
  char *args_bytes =
      NPVariantsToWireFormat(npp_, args, arg_count, NULL, &args_length);
  if ((NULL == args_bytes || 0 == args_length) && 0 != arg_count) {
    return false;
  }
  int32_t success;
  // Allocate space for the return npvariant.
  // TODO(sehr): this should really be allocated by the invoked method.
  char ret_bytes[kNPVariantSizeMax];
  nacl_abi_size_t ret_length =
      static_cast<nacl_abi_size_t>(sizeof(ret_bytes));
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_InvokeDefault(bridge->channel(),
                                               NPPToWireFormat(npp_),
                                               capability_.size(),
                                               capability_.char_addr(),
                                               args_length,
                                               args_bytes,
                                               static_cast<int32_t>(arg_count),
                                               &success,
                                               &ret_length,
                                               ret_bytes);
  // Free the serialized args.
  delete args_bytes;
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  // If the RPC was successful, get the result.
  if (success &&
      WireFormatToNPVariants(npp_, ret_bytes, ret_length, 1, variant)) {
    DebugPrintf("InvokeDefault(%p) succeeded: %s\n",
                reinterpret_cast<void*>(this),
                FormatNPVariant(variant));
    return true;
  }
  DebugPrintf("InvokeDefault(%p) failed.\n", reinterpret_cast<void*>(this));
  return false;
}

bool NPObjectProxy::HasProperty(NPIdentifier id) {
  DebugPrintf("HasProperty(%p, %s)\n",
              reinterpret_cast<void*>(this), FormatNPIdentifier(id));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t success;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_HasProperty(bridge->channel(),
                                             NPPToWireFormat(npp_),
                                             capability_.size(),
                                             capability_.char_addr(),
                                             NPIdentifierToWireFormat(id),
                                             &success);
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  return success ? true : false;
}

bool NPObjectProxy::GetProperty(NPIdentifier id, NPVariant* variant) {
  DebugPrintf("GetProperty(%p, %s)\n",
              reinterpret_cast<void*>(this),
              FormatNPIdentifier(id));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  char ret_bytes[kNPVariantSizeMax];
  uint32_t ret_length = static_cast<uint32_t>(sizeof(ret_bytes));
  int32_t success;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_GetProperty(bridge->channel(),
                                             NPPToWireFormat(npp_),
                                             capability_.size(),
                                             capability_.char_addr(),
                                             NPIdentifierToWireFormat(id),
                                             &success,
                                             &ret_length,
                                             ret_bytes);
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  // If the RPC was successful, get the result.
  if (success &&
      WireFormatToNPVariants(npp_, ret_bytes, ret_length, 1, variant)) {
    DebugPrintf("GetProperty(%p) succeeded: %s\n",
                reinterpret_cast<void*>(this),
                FormatNPVariant(variant));
    return true;
  }
  return false;
}

bool NPObjectProxy::SetProperty(NPIdentifier id, const NPVariant* value) {
  DebugPrintf("SetProperty(%p, %s, %s)\n",
              reinterpret_cast<void*>(this),
              FormatNPIdentifier(id),
              FormatNPVariant(value));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  // Serialize the argument.
  nacl_abi_size_t arg_length = kNaClAbiSizeTMax;
  char *arg_bytes =
      NPVariantsToWireFormat(npp_, value, 1, NULL, &arg_length);
  if (NULL == arg_bytes || 0 == arg_length) {
    return false;
  }
  int32_t success;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_SetProperty(bridge->channel(),
                                             NPPToWireFormat(npp_),
                                             capability_.size(),
                                             capability_.char_addr(),
                                             NPIdentifierToWireFormat(id),
                                             arg_length,
                                             arg_bytes,
                                             &success);
  // Free the serialized args.
  delete arg_bytes;
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  return success ? true : false;
}

bool NPObjectProxy::RemoveProperty(NPIdentifier id) {
  DebugPrintf("RemoveProperty(%p, %s)\n",
              reinterpret_cast<void*>(this),
              FormatNPIdentifier(id));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t success;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_RemoveProperty(bridge->channel(),
                                                NPPToWireFormat(npp_),
                                                capability_.size(),
                                                capability_.char_addr(),
                                                NPIdentifierToWireFormat(id),
                                                &success);
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  return success ? true : false;
}

bool NPObjectProxy::Enumerate(NPIdentifier** identifiers,
                              uint32_t* identifier_count) {
  UNREFERENCED_PARAMETER(identifiers);
  DebugPrintf("Enumerate(%p)\n", reinterpret_cast<void*>(this));

  // Initialize for errors.
  *identifiers = NULL;
  *identifier_count = 0;
  // Set up for the RPC.
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  char idents[kNPVariantSizeMax];
  nacl_abi_size_t idents_size = static_cast<nacl_abi_size_t>(sizeof(idents));
  int32_t success;
  int32_t ident_count;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_Enumerate(bridge->channel(),
                                           NPPToWireFormat(npp_),
                                           capability_.size(),
                                           capability_.char_addr(),
                                           &success,
                                           &idents_size,
                                           idents,
                                           &ident_count);
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result || !success) {
    return false;
  }
  // Check the validity of the return values.
  if (0 > ident_count) {
    return false;
  }
  NPIdentifier* identifier_array =
      static_cast<NPIdentifier*>(::NPN_MemAlloc(idents_size));
  if (NULL == identifier_array) {
    return false;
  }
  // Deserialize the NPIdentifiers.
  *identifiers = identifier_array;
  *identifier_count = static_cast<uint32_t>(ident_count);
  nacl_abi_size_t next_id_offset = 0;
  for (int32_t i = 0; i < ident_count; ++i) {
    if (idents_size <= next_id_offset) {
      // Did not get enough bytes to store the returned identifiers.
      return false;
    }
    int32_t* wire_ident = reinterpret_cast<int32_t*>(&idents[next_id_offset]);
    identifier_array[i] = WireFormatToNPIdentifier(*wire_ident);
    next_id_offset += sizeof(int32_t);
  }
  return true;
}

bool NPObjectProxy::Construct(const NPVariant* args,
                              uint32_t arg_count,
                              NPVariant* variant) {
  DebugPrintf("Construct(%p, %s, %u)\n",
              reinterpret_cast<void*>(this),
              FormatNPVariantVector(args, arg_count),
              static_cast<unsigned int>(arg_count));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  // Serialize the argument vector.
  nacl_abi_size_t args_length = kNaClAbiSizeTMax;
  char *args_bytes =
      NPVariantsToWireFormat(npp_, args, arg_count, NULL, &args_length);
  if ((NULL == args_bytes || 0 == args_length) && 0 != arg_count) {
    return false;
  }
  char ret_bytes[kNPVariantSizeMax];
  nacl_abi_size_t ret_length =
      static_cast<nacl_abi_size_t>(sizeof(ret_bytes));
  int32_t success;
  // Perform the RPC.
  NaClSrpcError srpc_result =
      NPObjectStubRpcClient::NPN_Construct(bridge->channel(),
                                           NPPToWireFormat(npp_),
                                           capability_.size(),
                                           capability_.char_addr(),
                                           args_length,
                                           args_bytes,
                                           static_cast<int32_t>(arg_count),
                                           &success,
                                           &ret_length,
                                           ret_bytes);
  // Free the serialized args.
  delete args_bytes;
  // Check that the RPC layer worked correctly.
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return false;
  }
  // If the RPC was successful, get the result.
  if (success &&
      WireFormatToNPVariants(npp_, ret_bytes, ret_length, 1, variant)) {
    DebugPrintf("Construct(%p) succeeded: %s\n",
                reinterpret_cast<void*>(this),
                FormatNPVariant(variant));
    return true;
  }
  return false;
}

void NPObjectProxy::SetException(const NPUTF8* message) {
  DebugPrintf("SetException(%p, %s)\n",
              reinterpret_cast<void*>(this),
              message);

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  // Perform the RPC.
  NPObjectStubRpcClient::NPN_SetException(bridge->channel(),
                                          capability_.size(),
                                          capability_.char_addr(),
                                          const_cast<NPUTF8*>(message));
}

}  // namespace nacl
