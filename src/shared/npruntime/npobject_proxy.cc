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

#include <stdarg.h>

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

NPObject* Alloc(NPP npp, NPClass* aClass) {
  return nacl::NPObjectProxy::GetLastAllocated();
}

void Deallocate(NPObject* object) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
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

bool Enumeration(NPObject* object, NPIdentifier* *value, uint32_t* count) {
  nacl::NPObjectProxy* npobj = static_cast<nacl::NPObjectProxy*>(object);
  return npobj->Enumeration(value, count);
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
  ::Enumeration,
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

NPObjectProxy::NPObjectProxy(NPBridge* bridge, const NPCapability& capability)
    : bridge_(bridge) {
  capability_.CopyFrom(capability);
  last_allocated = this;
  NPN_CreateObject(bridge->npp(), &np_class);
}

NPObjectProxy::~NPObjectProxy() {
  if (NULL == bridge_) {
    return;
  }
  RpcHeader request;
  request.type = RPC_DEALLOCATE;
  IOVec vecv[2];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  int length;
  bridge_->Request(&request, vecv, vecp - vecv, &length);
  bridge_->RemoveProxy(this);
}

void NPObjectProxy::Invalidate() {
  DebugPrintf("Invalidate\n");
  // Note Invalidate() can be called after NPP_Destroy() is called.
  if (NULL == bridge_) {
    return;
  }
  RpcHeader request;
  request.type = RPC_INVALIDATE;
  IOVec vecv[2];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  int length;
  bridge_->Request(&request, vecv, vecp - vecv, &length);
}

bool NPObjectProxy::HasMethod(NPIdentifier name) {
  DebugPrintf("HasMethod %p\n", name);
  RpcStack stack(bridge_);
  if (NULL == stack.Push(name)) {
    return false;
  }
  RpcHeader request;
  request.type = RPC_HAS_METHOD;
  IOVec vecv[4];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = &name;
  vecp->length = sizeof name;
  ++vecp;
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (reply == NULL) {
    return false;
  }
  return reply->error_code ? true : false;
}

bool NPObjectProxy::Invoke(NPIdentifier name,
                           const NPVariant* args,
                           uint32_t arg_count,
                           NPVariant* variant) {
  char* sname = NPN_UTF8FromIdentifier(name);
  DebugPrintf("Invoke %p %s\n", name, sname);
  NPN_MemFree(sname);

  RpcStack stack(bridge_);
  char converted_args[kNPVariantSizeMax * kParamMax];
  // Avoid stack overflow if too many parameters are passed.
  if (kParamMax < arg_count) {
    return false;
  }
  if (bridge_->peer_npvariant_size() != sizeof(NPVariant)) {
    ConvertNPVariants(args, converted_args,
                      bridge_->peer_npvariant_size(),
                      arg_count);
  }
  if (NULL == stack.Push(name)) {
    return false;
  }
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NULL == stack.Push(args[i], true)) {
      return false;
    }
  }
  RpcHeader request;
  request.type = RPC_INVOKE;
  request.error_code = arg_count;
  IOVec vecv[5];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = &name;
  vecp->length = sizeof name;
  ++vecp;
  if (0 < arg_count) {
    if (sizeof(NPVariant) == bridge_->peer_npvariant_size()) {
      vecp->base = const_cast<NPVariant*>(args);
      vecp->length = arg_count * sizeof(NPVariant);
    } else {
      vecp->base = converted_args;
      vecp->length = arg_count * bridge_->peer_npvariant_size();
    }
    ++vecp;
  }
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (NULL == reply) {
    return false;
  }
  RpcArg result(bridge_, reply, length);
  result.Step(sizeof(RpcHeader));
  if (NPERR_NO_ERROR != reply->error_code) {
    result.StepOption(sizeof(NPVariant));
    *variant = *result.GetVariant(false);
    return true;
  }
  return false;
}

bool NPObjectProxy::InvokeDefault(const NPVariant* args,
                                  uint32_t arg_count,
                                  NPVariant* variant) {
  DebugPrintf("InvokeDefault\n");
  RpcStack stack(bridge_);
  char converted_args[kNPVariantSizeMax * kParamMax];
  // Avoid stack overflow if too many parameters are passed.
  if (kParamMax < arg_count) {
    return false;
  }
  if (sizeof(NPVariant) != bridge_->peer_npvariant_size()) {
    ConvertNPVariants(args, converted_args,
                      bridge_->peer_npvariant_size(),
                      arg_count);
  }
  for (uint32_t i = 0; i < arg_count; ++i) {
    if (NULL == stack.Push(args[i], true)) {
      return false;
    }
  }
  RpcHeader request;
  request.type = RPC_INVOKE_DEFAULT;
  request.error_code = arg_count;
  IOVec vecv[4];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  if (0 < arg_count) {
    if (sizeof(NPVariant) == bridge_->peer_npvariant_size()) {
      vecp->base = const_cast<NPVariant*>(args);
      vecp->length = arg_count * sizeof(NPVariant);
    } else {
      vecp->base = converted_args;
      vecp->length = arg_count * bridge_->peer_npvariant_size();
    }
    ++vecp;
  }
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (NULL == reply) {
    return false;
  }
  RpcArg result(bridge_, reply, length);
  result.Step(sizeof(RpcHeader));
  if (NPERR_NO_ERROR != reply->error_code) {
    result.StepOption(sizeof(NPVariant));
    *variant = *result.GetVariant(false);
    return true;
  }
  return false;
}

bool NPObjectProxy::HasProperty(NPIdentifier name) {
  DebugPrintf("HasProperty %p\n", name);
  RpcStack stack(bridge_);
  if (NULL == stack.Push(name)) {
    return false;
  }
  RpcHeader request;
  request.type = RPC_HAS_PROPERTY;
  IOVec vecv[4];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = &name;
  vecp->length = sizeof name;
  ++vecp;
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (NULL == reply) {
    return false;
  }
  return reply->error_code ? true : false;
}

bool NPObjectProxy::GetProperty(NPIdentifier name, NPVariant* variant) {
  DebugPrintf("GetProperty %p\n", name);
  RpcStack stack(bridge_);
  if (NULL == stack.Push(name)) {
    return false;
  }
  RpcHeader request;
  request.type = RPC_GET_PROPERTY;
  IOVec vecv[4];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = &name;
  vecp->length = sizeof name;
  ++vecp;
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (NULL == reply) {
    return false;
  }
  RpcArg result(bridge_, reply, length);
  result.Step(sizeof(RpcHeader));
  if (NPERR_NO_ERROR != reply->error_code) {
    result.StepOption(sizeof(NPVariant));
    *variant = *result.GetVariant(false);
    return true;
  }
  return false;
}

bool NPObjectProxy::SetProperty(NPIdentifier name, const NPVariant* value) {
  DebugPrintf("SetProperty %p\n", name);
  RpcStack stack(bridge_);
  char converted_value[kNPVariantSizeMax];
  if (sizeof(NPVariant) != bridge_->peer_npvariant_size()) {
    ConvertNPVariants(value, converted_value,
                      bridge_->peer_npvariant_size(),
                      1);
  }
  if (NULL == stack.Push(name)) {
    return false;
  }
  if (NULL == stack.Push(*value, true)) {
    return false;
  }
  RpcHeader request;
  request.type = RPC_SET_PROPERTY;
  IOVec vecv[5];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = &name;
  vecp->length = sizeof name;
  ++vecp;
  if (sizeof(NPVariant) == bridge_->peer_npvariant_size()) {
    vecp->base = const_cast<NPVariant*>(value);
    vecp->length = sizeof(NPVariant);
  } else {
    vecp->base = converted_value;
    vecp->length = bridge_->peer_npvariant_size();
  }
  ++vecp;
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (NULL == reply) {
    return false;
  }
  return reply->error_code ? true : false;
}

bool NPObjectProxy::RemoveProperty(NPIdentifier name) {
  DebugPrintf("RemoveProperty %p\n", name);
  RpcStack stack(bridge_);
  if (NULL == stack.Push(name)) {
    return false;
  }
  RpcHeader request;
  request.type = RPC_REMOVE_PROPERTY;
  IOVec vecv[4];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = &name;
  vecp->length = sizeof name;
  ++vecp;
  vecp = stack.SetIOVec(vecp);
  int length;
  RpcHeader* reply = bridge_->Request(&request, vecv, vecp - vecv, &length);
  if (NULL == reply) {
    return false;
  }
  return reply->error_code ? true : false;
}

bool NPObjectProxy::Enumeration(NPIdentifier* *value, uint32_t* count) {
  // TODO(sehr): shouldn't this API do something?
  return false;
}

bool NPObjectProxy::Construct(const NPVariant* args,
                              uint32_t arg_count,
                              NPVariant* result) {
  // TODO(sehr): shouldn't this API do something?
  return false;
}

void NPObjectProxy::SetException(const NPUTF8* message) {
  DebugPrintf("SetException\n");
  RpcHeader request;
  request.type = RPC_SET_EXCEPTION;
  IOVec vecv[3];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = &capability_;
  vecp->length = sizeof capability_;
  ++vecp;
  vecp->base = const_cast<NPUTF8*>(message);
  vecp->length = strlen(message) + 1;
  ++vecp;
  bridge_->Request(&request, vecv, vecp - vecv, NULL);
}

int NPObjectProxy::SetException(RpcHeader* request, int len) {
  RpcArg arg(bridge_, request, len);
  arg.Step(sizeof(RpcHeader));
  arg.Step(sizeof(NPCapability));
  const NPUTF8* message = arg.GetString();
  NPN_SetException(this, message ? message : "");
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = request;
  vecp->length = sizeof(RpcHeader);
  ++vecp;
  return bridge_->Respond(request, vecv, vecp - vecv);
}

}  // namespace nacl
