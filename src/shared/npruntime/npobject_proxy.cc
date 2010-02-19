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
#ifdef __native_client__
#include "native_client/src/shared/npruntime/npnavigator.h"
using nacl::NPNavigator;
#else
using nacl::NPBridge;
#endif  // __native_client__

namespace {

// TODO(sehr): This should be moved when it no longer conflicts with
// the Pepper changelist.
static const int kNPVariantOptionalMax = 16 * 1024;

// These methods populate the NPClass for an object proxy:
// Alloc, Deallocate, Invalidate, HasMethod, Invoke, InvokeDefault, HasProperty,
// GetProperty, SetProperty, RemoveProperty, Enumerate, and Construct.
// They simply extract the proxy address and invoke the corresponding
// function on that object.
NPObject* Alloc(NPP npp, NPClass* aClass) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(aClass);

  return nacl::NPObjectProxy::GetLastAllocated();
}

void Deallocate(NPObject* object) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  nacl::DebugPrintf("Deallocate(%p)\n", reinterpret_cast<void*>(object));
  delete npobj;
}

// Invalidate is called after NPP_Destroy...
void Invalidate(NPObject* object) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->Invalidate();
}

bool HasMethod(NPObject* object, NPIdentifier name) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->HasMethod(name);
}

bool Invoke(NPObject* object,
            NPIdentifier name,
            const NPVariant* args,
            uint32_t arg_count,
            NPVariant* result) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->Invoke(name, args, arg_count, result);
}

bool InvokeDefault(NPObject* object,
                   const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->InvokeDefault(args, arg_count, result);
}

bool HasProperty(NPObject* object, NPIdentifier name) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->HasProperty(name);
}

bool GetProperty(NPObject* object, NPIdentifier name, NPVariant* result) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->GetProperty(name, result);
}

bool SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->SetProperty(name, value);
}

bool RemoveProperty(NPObject* object, NPIdentifier name) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->RemoveProperty(name);
}

bool Enumerate(NPObject* object, NPIdentifier* *value, uint32_t* count) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->Enumerate(value, count);
}

bool Construct(NPObject* object,
               const NPVariant* args,
               uint32_t arg_count,
               NPVariant* result) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->Construct(args, arg_count, result);
}

// TODO(sehr): we need to centralize conversion to/from NPP wire format.
int32_t NPPToWireFormat(NPP npp) {
#ifdef __native_client__
  return NPNavigator::GetPluginNPP(npp);
#else
  return NPBridge::NppToInt(npp);
#endif
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
  DebugPrintf("&NPObjectProxy(%p)\n", reinterpret_cast<void*>(this));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }

  NPObjectStubRpcClient::NPN_Deallocate(bridge->channel(),
                                        ::NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr());
  bridge->RemoveProxy(this);
}

void NPObjectProxy::Deallocate() {
  DebugPrintf("Deallocate\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  NPObjectStubRpcClient::NPN_Deallocate(bridge->channel(),
                                        ::NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr());
}

void NPObjectProxy::Invalidate() {
  DebugPrintf("Invalidate\n");

  // Note Invalidate() can be called after NPP_Destroy() is called.
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  NPObjectStubRpcClient::NPN_Invalidate(bridge->channel(),
                                        ::NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr());
}

bool NPObjectProxy::HasMethod(NPIdentifier name) {
  DebugPrintf("HasMethod(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(")\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t error_code;
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_HasMethod(bridge->channel(),
                                           ::NPPToWireFormat(npp_),
                                           capability_.size(),
                                           capability_.char_addr(),
                                           NPBridge::NpidentifierToInt(name),
                                           &error_code)) {
    return false;
  }
  return error_code ? true : false;
}

bool NPObjectProxy::Invoke(NPIdentifier name,
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

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  // TODO(sehr): this is really verbose.  New RpcArg constructor.
  char fixed[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t fixed_size =
      static_cast<nacl_abi_size_t>(sizeof(fixed));
  char optional[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t optional_size =
      static_cast<nacl_abi_size_t>(sizeof(optional));
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariantArray(args, arg_count)) {
    return false;
  }
  int32_t ret_bool;
  char ret_fixed[100 * kNPVariantSizeMax];
  nacl_abi_size_t ret_fixed_size =
      static_cast<nacl_abi_size_t>(sizeof(ret_fixed));
  char ret_optional[100 * kNPVariantSizeMax];
  nacl_abi_size_t ret_optional_size =
      static_cast<nacl_abi_size_t>(sizeof(ret_optional));
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_Invoke(bridge->channel(),
                                        ::NPPToWireFormat(npp_),
                                        capability_.size(),
                                        capability_.char_addr(),
                                        NPBridge::NpidentifierToInt(name),
                                        fixed_size,
                                        fixed,
                                        optional_size,
                                        optional,
                                        static_cast<int32_t>(arg_count),
                                        &ret_bool,
                                        &ret_fixed_size,
                                        ret_fixed,
                                        &ret_optional_size,
                                        ret_optional)) {
    DebugPrintf("    invoke error\n");
    return false;
  }
  if (ret_bool) {
    RpcArg rets(npp_,
                ret_fixed,
                ret_fixed_size,
                ret_optional,
                ret_optional_size);
    *variant = *rets.GetVariant(true);
    DebugPrintf("Invoke(%p, ", reinterpret_cast<void*>(this));
    PrintIdent(name);
    printf(") succeeded: ");
    PrintVariant(variant);
    printf("\n");
    return true;
  }
  DebugPrintf("Invoke(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(") failed.\n");
  return false;
}

bool NPObjectProxy::InvokeDefault(const NPVariant* args,
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

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    printf("bridge was NULL\n");
    return false;
  }
  char fixed[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t fixed_size = static_cast<nacl_abi_size_t>(sizeof(fixed));
  char optional[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t optional_size =
      static_cast<nacl_abi_size_t>(sizeof(optional));
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariantArray(args, arg_count)) {
    printf("Args didn't fit\n");
    return false;
  }
  int32_t ret_bool;
  char ret_fixed[kNPVariantSizeMax];
  nacl_abi_size_t ret_fixed_size =
      static_cast<nacl_abi_size_t>(sizeof(ret_fixed));
  char ret_optional[kNPVariantOptionalMax];
  nacl_abi_size_t ret_optional_size =
      static_cast<nacl_abi_size_t>(sizeof(ret_optional));
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_InvokeDefault(bridge->channel(),
                                               ::NPPToWireFormat(npp_),
                                               capability_.size(),
                                               capability_.char_addr(),
                                               fixed_size,
                                               fixed,
                                               optional_size,
                                               optional,
                                               static_cast<int32_t>(arg_count),
                                               &ret_bool,
                                               &ret_fixed_size,
                                               ret_fixed,
                                               &ret_optional_size,
                                               ret_optional)) {
    printf("RPC failed\n");
    return false;
  }
  if (ret_bool) {
    RpcArg rets(npp_,
                ret_fixed,
                ret_fixed_size,
                ret_optional,
                ret_optional_size);
    *variant = *rets.GetVariant(true);
    DebugPrintf("InvokeDefault(%p) succeeded: ",
                reinterpret_cast<void*>(this));
    PrintVariant(variant);
    printf("\n");
    return true;
  }
  DebugPrintf("InvokeDefault(%p) failed: ", reinterpret_cast<void*>(this));
  return false;
}

bool NPObjectProxy::HasProperty(NPIdentifier name) {
  DebugPrintf("HasProperty(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(")\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t error_code;
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_HasProperty(bridge->channel(),
                                             ::NPPToWireFormat(npp_),
                                             capability_.size(),
                                             capability_.char_addr(),
                                             NPBridge::NpidentifierToInt(name),
                                             &error_code)) {
    return false;
  }
  return error_code ? true : false;
}

bool NPObjectProxy::GetProperty(NPIdentifier name, NPVariant* variant) {
  DebugPrintf("GetProperty(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(")\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t error_code;
  char ret_fixed[kNPVariantSizeMax];
  uint32_t ret_fixed_size = static_cast<uint32_t>(sizeof(ret_fixed));
  char ret_optional[kNPVariantOptionalMax];
  uint32_t ret_optional_size = static_cast<uint32_t>(sizeof(ret_optional));
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_GetProperty(bridge->channel(),
                                             ::NPPToWireFormat(npp_),
                                             capability_.size(),
                                             capability_.char_addr(),
                                             NPBridge::NpidentifierToInt(name),
                                             &error_code,
                                             &ret_fixed_size,
                                             ret_fixed,
                                             &ret_optional_size,
                                             ret_optional)) {
    DebugPrintf("GetProperty failed\n");
    return false;
  }
  if (1 == error_code) {
    RpcArg rets(npp_,
                ret_fixed,
                ret_fixed_size,
                ret_optional,
                ret_optional_size);
    *variant = *rets.GetVariant(true);
    DebugPrintf("GetProperty(%p, ", reinterpret_cast<void*>(this));
    PrintIdent(name);
    printf(") succeeded: ");
    PrintVariant(variant);
    printf("\n");
    return true;
  }
  return false;
}

bool NPObjectProxy::SetProperty(NPIdentifier name, const NPVariant* value) {
  DebugPrintf("SetProperty(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(", ");
  PrintVariant(value);
  printf(")\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  char      fixed[kNPVariantSizeMax];
  uint32_t  fixed_size = static_cast<uint32_t>(sizeof(fixed));
  char      optional[kNPVariantOptionalMax];
  uint32_t  optional_size = static_cast<uint32_t>(sizeof(optional));
  RpcArg    vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariant(value)) {
    return false;
  }
  int32_t error_code;
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_SetProperty(bridge->channel(),
                                             ::NPPToWireFormat(npp_),
                                             capability_.size(),
                                             capability_.char_addr(),
                                             NPBridge::NpidentifierToInt(name),
                                             fixed_size,
                                             fixed,
                                             optional_size,
                                             optional,
                                             &error_code)) {
    return false;
  }
  return error_code ? true : false;
}

bool NPObjectProxy::RemoveProperty(NPIdentifier name) {
  DebugPrintf("RemoveProperty(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(")\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int32_t error_code;
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_RemoveProperty(
          bridge->channel(),
          ::NPPToWireFormat(npp_),
          capability_.size(),
          capability_.char_addr(),
          NPBridge::NpidentifierToInt(name),
          &error_code)) {
    return false;
  }
  return error_code ? true : false;
}

bool NPObjectProxy::Enumerate(NPIdentifier** identifiers,
                              uint32_t* identifier_count) {
  UNREFERENCED_PARAMETER(identifiers);
  DebugPrintf("Enumerate(%p)\n", reinterpret_cast<void*>(this));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  // TODO(sehr): there are some identifier copying issues here, etc.
  char idents[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t idents_size = static_cast<nacl_abi_size_t>(sizeof(idents));
  int32_t error_code;
  int32_t ident_count;
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_Enumerate(bridge->channel(),
                                           ::NPPToWireFormat(npp_),
                                           capability_.size(),
                                           capability_.char_addr(),
                                           &error_code,
                                           &idents_size,
                                           idents,
                                           &ident_count)) {
    return false;
  }
  // TODO(sehr): we're still not copying the identifier list, etc.
  // Again, SRPC only handles int32_t, so we need a cast.
  *identifier_count = static_cast<uint32_t>(ident_count);
  return false;
}

bool NPObjectProxy::Construct(const NPVariant* args,
                              uint32_t arg_count,
                              NPVariant* result) {
  DebugPrintf("Construct(%p, %u)\n",
              reinterpret_cast<void*>(this),
              static_cast<unsigned int>(arg_count));

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  char fixed[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t fixed_size =
      static_cast<nacl_abi_size_t>(sizeof(fixed));
  char optional[kNPVariantSizeMax * kParamMax];
  nacl_abi_size_t optional_size =
      static_cast<nacl_abi_size_t>(sizeof(optional));
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariantArray(args, arg_count)) {
    return false;
  }
  int32_t error_code;
  char ret_fixed[kNPVariantSizeMax];
  nacl_abi_size_t ret_fixed_size =
      static_cast<nacl_abi_size_t>(sizeof(optional));
  char ret_optional[kNPVariantSizeMax];
  nacl_abi_size_t ret_optional_size =
      static_cast<nacl_abi_size_t>(sizeof(optional));
  if (NACL_SRPC_RESULT_OK !=
      NPObjectStubRpcClient::NPN_Construct(bridge->channel(),
                                           ::NPPToWireFormat(npp_),
                                           capability_.size(),
                                           capability_.char_addr(),
                                           fixed_size,
                                           fixed,
                                           optional_size,
                                           optional,
                                           static_cast<int32_t>(arg_count),
                                           &error_code,
                                           &ret_fixed_size,
                                           ret_fixed,
                                           &ret_optional_size,
                                           ret_optional)) {
    return false;
  }
  if (NPERR_NO_ERROR != error_code) {
    RpcArg rets(npp_,
                ret_fixed,
                ret_fixed_size,
                ret_optional,
                ret_optional_size);
    *result = *rets.GetVariant(true);
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
  NPObjectStubRpcClient::NPN_SetException(bridge->channel(),
                                          capability_.size(),
                                          capability_.char_addr(),
                                          const_cast<NPUTF8*>(message));
}

}  // namespace nacl
