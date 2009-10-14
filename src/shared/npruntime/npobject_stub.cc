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

#include "native_client/src/shared/npruntime/npobject_stub.h"

#include <stdarg.h>

#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf(stderr, "@@@ STUB ");

  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

namespace nacl {

NPObjectStub::NPObjectStub(NPP npp, NPObject* object)
    : npp_(npp),
      object_(object) {
  NPN_RetainObject(object_);
}

NPObjectStub::~NPObjectStub() {
  NPN_ReleaseObject(object_);
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  bridge->RemoveStub(this);
}

NPObjectStub* NPObjectStub::Lookup(NaClSrpcChannel* channel, RpcArg* arg) {
  NPBridge* bridge = NPBridge::LookupBridge(channel);
  NPCapability* capability = arg->GetCapability();
  return bridge->GetStub(*capability);
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
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("Deallocate\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("Invalidate\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("HasMethod\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("Invoke\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  RpcArg arg23(stub->npp(), inputs[2], inputs[3]);
  uint32_t arg_count = inputs[4]->u.ival;
  const NPVariant* args = arg23.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  outputs[0]->u.ival = stub->InvokeImpl(name, args, arg_count, &variant);
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
  DebugPrintf("InvokeDefault\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("HasProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("GetProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(inputs[1]->u.ival);
  NPVariant variant;
  // Invoke the implementation.
  outputs[0]->u.ival = stub->GetPropertyImpl(name, &variant);
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
// (char[]) value fixed portion
// (char[]) value optional portion
// outputs:
// (int) success
NaClSrpcError NPObjectStub::SetProperty(NaClSrpcChannel* channel,
                                        NaClSrpcArg** inputs,
                                        NaClSrpcArg** outputs) {
  DebugPrintf("SetProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("RemoveProperty\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("Enumerate\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  DebugPrintf("Construct\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
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
  UNREFERENCED_PARAMETER(outputs);
  DebugPrintf("SetException\n");
  RpcArg arg0(NULL, inputs[0]);
  NPObjectStub* stub = Lookup(channel, &arg0);
  const NPUTF8* message = inputs[1]->u.sval;
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

}  // namespace nacl
