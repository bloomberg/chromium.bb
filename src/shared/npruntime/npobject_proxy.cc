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
// Ojbect proxying allows scripting objects across two different processes.
// The "proxy" side is in the process scripting the object.
// The "stub" side is in the process implementing the object.

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"

#include <stdarg.h>

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf(stderr, "@@@ PROXY ");

  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

namespace {

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
  DebugPrintf("Deallocate(%p)\n", reinterpret_cast<void*>(object));
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
  DebugPrintf("NPObjectProxy\n");

  capability_.CopyFrom(capability);
  last_allocated = this;
  NPN_CreateObject(npp_, &np_class);
}

NPObjectProxy::~NPObjectProxy() {
  DebugPrintf("~NPObjectProxy\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  NaClSrpcInvokeByName(bridge->channel(),
                       "NPN_Deallocate",
                       sizeof capability_,
                       &capability_);
  bridge->RemoveProxy(this);
}

void NPObjectProxy::Deallocate() {
  DebugPrintf("Deallocate\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  NaClSrpcInvokeByName(bridge->channel(),
                       "NPN_Deallocate",
                       sizeof capability_,
                       &capability_);
}

void NPObjectProxy::Invalidate() {
  DebugPrintf("Invalidate\n");

  // Note Invalidate() can be called after NPP_Destroy() is called.
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return;
  }
  NaClSrpcInvokeByName(bridge->channel(),
                       "NPN_Invalidate",
                       sizeof capability_,
                       &capability_);
}

bool NPObjectProxy::HasMethod(NPIdentifier name) {
  DebugPrintf("HasMethod(%p, ", reinterpret_cast<void*>(this));
  PrintIdent(name);
  printf(")\n");

  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  if (NULL == bridge) {
    return false;
  }
  int error_code;
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_HasMethod",
                           sizeof capability_,
                           &capability_,
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
  uint32_t fixed_size = static_cast<uint32_t>(sizeof(fixed));
  char optional[kNPVariantSizeMax * kParamMax];
  uint32_t optional_size = static_cast<uint32_t>(sizeof(optional));
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariantArray(args, arg_count)) {
    return false;
  }
  int ret_bool;
  char ret_fixed[100 * kNPVariantSizeMax];
  uint32_t ret_fixed_size = static_cast<uint32_t>(sizeof(ret_fixed));
  char ret_optional[100 * kNPVariantSizeMax];
  uint32_t ret_optional_size = static_cast<uint32_t>(sizeof(ret_optional));
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_Invoke",
                           sizeof capability_,
                           &capability_,
                           NPBridge::NpidentifierToInt(name),
                           fixed_size,
                           fixed,
                           optional_size,
                           optional,
                           arg_count,
                           &ret_bool,
                           ret_fixed_size,  // Need size set by return
                           ret_fixed,
                           ret_optional_size,  // Need size set by return
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
  printf(") failed.");
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
    return false;
  }
  char fixed[kNPVariantSizeMax * kParamMax];
  size_t fixed_size = sizeof(fixed);
  char optional[kNPVariantSizeMax * kParamMax];
  size_t optional_size = sizeof(optional);
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariantArray(args, arg_count)) {
    return false;
  }
  int error_code;
  char ret_fixed[kNPVariantSizeMax];
  uint32_t ret_fixed_size = static_cast<uint32_t>(sizeof(optional));
  char ret_optional[kNPVariantSizeMax];
  uint32_t ret_optional_size = static_cast<uint32_t>(sizeof(optional));
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_InvokeDefault",
                           sizeof capability_,
                           &capability_,
                           static_cast<uint32_t>(fixed_size),
                           fixed,
                           static_cast<uint32_t>(optional_size),
                           optional,
                           &error_code,
                           ret_fixed_size,  // Need size set by return
                           ret_fixed,
                           ret_optional_size,  // Need size set by return
                           ret_optional)) {
    return false;
  }
  if (NPERR_NO_ERROR != error_code) {
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
  int error_code;
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_HasProperty",
                           sizeof capability_,
                           &capability_,
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
  int error_code;
  char ret_fixed[kNPVariantSizeMax];
  uint32_t ret_fixed_size = static_cast<uint32_t>(sizeof(ret_fixed));
  char ret_optional[kNPVariantSizeMax];
  uint32_t ret_optional_size = static_cast<uint32_t>(sizeof(ret_optional));
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_GetProperty",
                           sizeof capability_,
                           &capability_,
                           NPBridge::NpidentifierToInt(name),
                           &error_code,
                           ret_fixed_size,  // Need size set by return
                           ret_fixed,
                           ret_optional_size,  // Need size set by return
                           ret_optional)) {
    return false;
  }
  if (NPERR_NO_ERROR != error_code) {
    RpcArg rets(npp_,
                ret_fixed,
                ret_fixed_size,
                ret_optional,
                ret_optional_size);
    *variant = *rets.GetVariant(true);
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
  char fixed[kNPVariantSizeMax];
  size_t fixed_size = sizeof(fixed);
  char optional[kNPVariantSizeMax];
  size_t optional_size = sizeof(optional);
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariant(value)) {
    return false;
  }
  int error_code;
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_SetProperty",
                           sizeof capability_,
                           &capability_,
                           NPBridge::NpidentifierToInt(name),
                           static_cast<uint32_t>(fixed_size),
                           fixed,
                           static_cast<uint32_t>(optional_size),
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
  int error_code;
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_RemoveProperty",
                           sizeof capability_,
                           &capability_,
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
  int error_code;
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_Enumerate",
                           sizeof capability_,
                           &capability_,
                           &error_code,
                           sizeof(idents),
                           idents,
                           identifier_count)) {
    return false;
  }
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
  size_t fixed_size = sizeof(fixed);
  char optional[kNPVariantSizeMax * kParamMax];
  size_t optional_size = sizeof(optional);
  RpcArg vars(npp_, fixed, fixed_size, optional, optional_size);
  if (!vars.PutVariantArray(args, arg_count)) {
    return false;
  }
  int error_code;
  char ret_fixed[kNPVariantSizeMax];
  uint32_t ret_fixed_size = static_cast<uint32_t>(sizeof(optional));
  char ret_optional[kNPVariantSizeMax];
  uint32_t ret_optional_size = static_cast<uint32_t>(sizeof(optional));
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(bridge->channel(),
                           "NPN_Construct",
                           sizeof capability_,
                           &capability_,
                           static_cast<uint32_t>(fixed_size),
                           fixed,
                           static_cast<uint32_t>(optional_size),
                           optional,
                           &error_code,
                           ret_fixed_size,  // Need size set by return
                           ret_fixed,
                           ret_optional_size,  // Need size set by return
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
  NaClSrpcInvokeByName(bridge->channel(),
                       "NPN_SetException",
                       sizeof capability_,
                       &capability_,
                       strlen(message) + 1,
                       const_cast<NPUTF8*>(message));
}

}  // namespace nacl
