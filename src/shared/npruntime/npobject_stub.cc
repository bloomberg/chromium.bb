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

#include <stdarg.h>

#include "native_client/src/shared/npruntime/nacl_util.h"

#include "native_client/src/shared/npruntime/npbridge.h"

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf (stderr, "@@@ STUB ");

  va_start (argptr, fmt);
  vfprintf (stderr, fmt, argptr);
  va_end (argptr);
  fflush(stderr);
}


namespace nacl {

NPObjectStub::NPObjectStub(NPBridge* bridge, NPObject* object)
    : bridge_(bridge),
      object_(object) {
  NPN_RetainObject(object_);
}

NPObjectStub::~NPObjectStub() {
  NPN_ReleaseObject(object_);
  bridge_->RemoveStub(this);
}

int NPObjectStub::Dispatch(RpcHeader* request, int len) {
  NPVariant variant;
  bool return_variant = false;

  RpcArg arg(bridge_, request, len);
  arg.Step(sizeof(RpcHeader));
  arg.Step(sizeof(NPCapability));

  switch (request->type) {
    case RPC_DEALLOCATE:
      request->error_code = true;
      break;
    case RPC_INVALIDATE:
      request->error_code = Invalidate(&arg);
      break;
    case RPC_HAS_METHOD:
      request->error_code = HasMethod(&arg);
      break;
    case RPC_INVOKE:
      return_variant = true;
      request->error_code = Invoke(request->error_code, &arg, &variant);
      break;
    case RPC_INVOKE_DEFAULT:
      return_variant = true;
      request->error_code = InvokeDefault(request->error_code, &arg, &variant);
      break;
    case RPC_HAS_PROPERTY:
      request->error_code = HasProperty(&arg);
      break;
    case RPC_GET_PROPERTY:
      return_variant = true;
      request->error_code = GetProperty(&arg, &variant);
      break;
    case RPC_SET_PROPERTY:
      request->error_code = SetProperty(&arg);
      break;
    case RPC_REMOVE_PROPERTY:
      request->error_code = RemoveProperty(&arg);
      break;
    case RPC_ENUMERATION:
      request->error_code = Enumeration(&arg);
      break;
    case RPC_CONSTRUCT:
      return_variant = true;
      request->error_code = Construct(request->error_code, &arg, &variant);
      break;
    default:
      return -1;
  }
  IOVec vecv[3];
  IOVec* vecp = vecv;
  vecp->base = request;
  vecp->length = sizeof(RpcHeader);
  ++vecp;

  arg.CloseUnusedHandles();

  RpcStack stack(bridge_);
  char converted_variant[kNPVariantSizeMax];
  if (request->error_code != false && return_variant) {
    if (bridge_->peer_npvariant_size() == sizeof(NPVariant)) {
      vecp->base = &variant;
      vecp->length = sizeof(variant);
    } else {
      ConvertNPVariants(&variant, converted_variant,
                        bridge_->peer_npvariant_size(),
                        1);
      vecp->base = converted_variant;
      vecp->length = bridge_->peer_npvariant_size();
    }
    ++vecp;
    stack.Push(variant, false);
    vecp = stack.SetIOVec(vecp);
  }
  int length = bridge_->Respond(request, vecv, vecp - vecv);
  if (request->error_code != false && return_variant &&
      NPVARIANT_IS_STRING(variant)) {
    // We cannot call NPN_ReleaseVariantValue() before bridge_->Respond()
    // completes since NPN_ReleaseVariantValue() changes the variant type to
    // NPVariantType_Void.
    NPN_ReleaseVariantValue(&variant);
  }
  if (request->type == RPC_DEALLOCATE) {
    delete this;
  }
  return length;
}

bool NPObjectStub::Invalidate(RpcArg* arg) {
  DebugPrintf("Invalidate\n");

  if (object_->_class && object_->_class->invalidate) {
    object_->_class->invalidate(object_);
    object_->referenceCount = 1;
  }
  return true;
}

bool NPObjectStub::HasMethod(RpcArg* arg) {
  arg->StepOption(sizeof(NPIdentifier));
  NPIdentifier name = arg->GetIdentifier();
  DebugPrintf("HasMethod: %p\n", name);

  return NPN_HasMethod(bridge_->npp(), object_, name);
}

bool NPObjectStub::Invoke(uint32_t arg_count, RpcArg* arg,
                          NPVariant* variant) {
  arg->StepOption(sizeof(NPIdentifier) + arg_count * sizeof(NPVariant));
  NPIdentifier name = arg->GetIdentifier();
  DebugPrintf("Invoke: %p\n", name);

  const NPVariant* args = NULL;
  if (0 < arg_count) {
    args = arg->GetVariant(true);
    for (uint32_t i = 1; i < arg_count; ++i) {
      arg->GetVariant(true);
    }
  }
  bool return_value = NPN_Invoke(bridge_->npp(), object_, name,
                                 args, arg_count, variant);
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NPVARIANT_IS_OBJECT(args[i])) {
      NPObject* object = NPVARIANT_TO_OBJECT(args[i]);
      NPN_ReleaseObject(object);
    }
  }
  return return_value;
}

bool NPObjectStub::InvokeDefault(uint32_t arg_count,
                                 RpcArg* arg,
                                 NPVariant* variant) {
  DebugPrintf("InvokeDefault\n");
  arg->StepOption(arg_count * sizeof(NPVariant));
  const NPVariant* args = NULL;
  if (0 < arg_count) {
    args = arg->GetVariant(true);
    for (uint32_t i = 1; i < arg_count; ++i) {
      arg->GetVariant(true);
    }
  }
  bool return_value = NPN_InvokeDefault(bridge_->npp(), object_,
                                        args, arg_count, variant);
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NPVARIANT_IS_OBJECT(args[i])) {
      NPObject* object = NPVARIANT_TO_OBJECT(args[i]);
      NPN_ReleaseObject(object);
    }
  }
  return return_value;
}

bool NPObjectStub::HasProperty(RpcArg* arg) {
  arg->StepOption(sizeof(NPIdentifier));
  NPIdentifier name = arg->GetIdentifier();
  DebugPrintf("HasProperty: %p\n", name);

  return NPN_HasProperty(bridge_->npp(), object_, name);
}

bool NPObjectStub::GetProperty(RpcArg* arg, NPVariant* variant) {
  arg->StepOption(sizeof(NPIdentifier));
  NPIdentifier name = arg->GetIdentifier();
  DebugPrintf("GetProperty: %p\n", name);

  return NPN_GetProperty(bridge_->npp(), object_, name, variant);
}

bool NPObjectStub::SetProperty(RpcArg* arg) {
  arg->StepOption(sizeof(NPIdentifier) + sizeof(NPVariant));
  NPIdentifier name = arg->GetIdentifier();
  DebugPrintf("SetProperty: %p\n", name);

  const NPVariant* variant = arg->GetVariant(true);
  bool return_value = NPN_SetProperty(bridge_->npp(), object_, name, variant);
  if (NPVARIANT_IS_OBJECT(*variant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(*variant);
    NPN_ReleaseObject(object);
  }
  return return_value;
}

bool NPObjectStub::RemoveProperty(RpcArg* arg) {
  arg->StepOption(sizeof(NPIdentifier));
  NPIdentifier name = arg->GetIdentifier();
  DebugPrintf("RemoveProperty: %p\n", name);

  return NPN_RemoveProperty(bridge_->npp(), object_, name);
}

bool NPObjectStub::Enumeration(RpcArg* arg) {
  return false;
}

bool NPObjectStub::Construct(uint32_t arg_count, RpcArg* arg,
                             NPVariant* variant) {
  return false;
}

void NPObjectStub::SetException(const NPUTF8* message) {
  RpcHeader request;
  request.type = RPC_SET_EXCEPTION;
  IOVec vecv[3];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  NPCapability capability = { GetPID(), object_ };
  vecp->base = &capability;
  vecp->length = sizeof capability;
  ++vecp;
  vecp->base = const_cast<NPUTF8*>(message);
  vecp->length = strlen(message) + 1;
  ++vecp;
  bridge_->Request(&request, vecv, vecp - vecv, NULL);
}

}  // namespace nacl
