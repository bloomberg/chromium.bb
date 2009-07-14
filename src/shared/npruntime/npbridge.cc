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

#include "native_client/src/shared/npruntime/npbridge.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/nacl_util.h"

namespace nacl {

NPBridge::NPBridge(NPP npp)
    : npp_(npp),
      peer_pid_(-1),
      peer_npvariant_size_(0),  // Must be set later
      channel_(kInvalidHtpHandle),
      waiting_since_(0),
      tag_(0),
      handle_count_(0),
      is_webkit_(false) {  // TBD
  Init();
}

NPBridge::~NPBridge() {
  while (!stub_map_.empty()) {
    std::map<NPObject*, NPObjectStub*>::iterator i = stub_map_.begin();
    NPObjectStub* stub = (*i).second;
    delete stub;
  }

  Close(channel_);

  // Note proxy objects are deleted by Navigator.
  for (std::map<const NPCapability, NPObjectProxy*>::iterator i =
          proxy_map_.begin();
       i != proxy_map_.end();
       ++i) {
    NPObjectProxy* proxy = (*i).second;
    proxy->Detach();
  }
}

NPObjectStub* NPBridge::LookupStub(NPObject* object) {
  assert(object);
  std::map<NPObject*, NPObjectStub*>::iterator i;
  i = stub_map_.find(object);
  if (i != stub_map_.end()) {
    return (*i).second;
  }
  return NULL;
}

NPObjectProxy* NPBridge::LookupProxy(const NPCapability& capability) {
  if (capability.object == NULL) {
    return NULL;
  }
  if (capability.pid == GetPID()) {
    return NULL;
  }
  std::map<const NPCapability, NPObjectProxy*>::iterator i;
  i = proxy_map_.find(capability);
  if (i != proxy_map_.end()) {
    return (*i).second;
  }
  return NULL;
}

int NPBridge::CreateStub(NPObject* object, NPCapability* cap) {
  NPObjectStub* stub = NULL;
  if (object != NULL) {
    if (NPObjectProxy::IsInstance(object)) {  // object can be a proxy
      NPObjectProxy* proxy = static_cast<NPObjectProxy*>(object);
      if (proxy->capability().pid == peer_pid()) {
        *cap = proxy->capability();
        return 0;
      }
    }
    stub = LookupStub(object);
    if (stub) {
      cap->pid = GetPID();
      cap->object = object;
      return 1;
    }
    stub = new(std::nothrow) NPObjectStub(this, object);
  }
  cap->pid = GetPID();
  if (!stub) {
    cap->object = NULL;
    return 0;
  }
  stub_map_[object] = stub;
  cap->object = object;
  return 1;
}

NPObject* NPBridge::CreateProxy(const NPCapability& capability) {
  if (capability.object == NULL) {
    return NULL;
  }
  if (capability.pid == GetPID()) {  // capability can be of my process
    std::map<NPObject*, NPObjectStub*>::iterator i;
    i = stub_map_.find(capability.object);
    if (i == stub_map_.end()) {
      return NULL;
    }
    NPN_RetainObject(capability.object);
    return capability.object;
  }

  std::map<const NPCapability, NPObjectProxy*>::iterator i;
  i = proxy_map_.find(capability);
  if (i != proxy_map_.end()) {
    NPObjectProxy* proxy = (*i).second;
    NPN_RetainObject(proxy);
    return proxy;
  }
  NPObjectProxy* proxy = new(std::nothrow) NPObjectProxy(this, capability);
  if (!proxy) {
    return NULL;
  }
  AddProxy(proxy);
  return proxy;
}

void NPBridge::AddProxy(NPObjectProxy* proxy) {
  proxy_map_[proxy->capability()] = proxy;
}

void NPBridge::RemoveProxy(NPObjectProxy* proxy) {
  proxy_map_.erase(proxy->capability());
}

NPObjectStub* NPBridge::GetStub(const NPCapability& capability) {
  if (capability.pid != GetPID()) {
    return NULL;
  }
  std::map<NPObject*, NPObjectStub*>::iterator i;
  i = stub_map_.find(capability.object);
  if (i == stub_map_.end()) {
    return NULL;
  }
  return (*i).second;
}

void NPBridge::RemoveStub(NPObjectStub* stub) {
  stub_map_.erase(stub->object());
}

RpcHeader* NPBridge::Request(RpcHeader* request, IOVec* iov, size_t iov_length,
                             int* length) {
  if (channel_ == kInvalidHtpHandle) {
    return NULL;
  }
  assert(iov[0].base == request);
  request->tag = ++tag_;
  request->pid = GetPID();

  HtpHeader message;
  message.iov = iov;
  message.iov_length = iov_length;
  message.handles = handles_;
  message.handle_count = handle_count_;
  if (SendDatagram(channel_, &message, 0) <
          static_cast<int>(sizeof(RpcHeader))) {
    clear_handle_count();
    return NULL;
  }
  return Wait(request, length);
}

RpcHeader* NPBridge::Wait(const RpcHeader* request, int* length) {
  RpcHeader* header = static_cast<RpcHeader*>(top());
  for (;;) {
    HtpHeader message;
    IOVec vec;
    vec.base = top();
    vec.length = GetFreeSize();
    message.iov = &vec;
    message.iov_length = 1;
    message.handles = handles_;
    message.handle_count = kHandleCountMax;
    clear_handle_count();

    waiting_since_ = time(NULL);
    int result = ReceiveDatagram(channel_, &message, 0);
    waiting_since_ = 0;
    if (result == -1) {
      Close(channel_);
      channel_ = kInvalidHtpHandle;
      return NULL;
    }
    if (result < static_cast<int>(sizeof(RpcHeader))) {
      for (size_t i = 0; i < message.handle_count; ++i) {
        Close(handles_[i]);
      }
      Close(channel_);
      channel_ = kInvalidHtpHandle;
      return NULL;
    }
    handle_count_ = message.handle_count;
    if (request && header->Equals(*request)) {
      // Received the response for the request.
      if (length != 0) {
        *length = result;
      }
      return header;
    }
    RpcStack stack(this);
    stack.Alloc(result);
    result = Dispatch(header, result);
    if (result == -1) {
      // Send back a truncated header to indicate failure.
      clear_handle_count();
      IOVec iov = {
        const_cast<RpcHeader*>(request),
        offsetof(RpcHeader, error_code)
      };
      Respond(request, &iov, 1);
    }
    if (header->type == RPC_DESTROY) {
      return NULL;
    }
  }
}

int NPBridge::Dispatch(RpcHeader* request, int len) {
  RpcArg arg(this, request, len);
  if (len < static_cast<int>(sizeof(RpcHeader) + sizeof(NPCapability))) {
    return -1;
  }
  arg.Step(sizeof(RpcHeader));
  const NPCapability* capability = arg.GetCapability();
  assert(capability);

  switch (request->type) {
    case RPC_SET_EXCEPTION: {
      NPObjectStub* stub = GetStub(*capability);
      if (stub != NULL) {
        return stub->Dispatch(request, len);
      }
      NPObjectProxy* proxy = LookupProxy(*capability);
      if (proxy != NULL) {
        return proxy->SetException(request, len);
      }
      break;
    }
    case RPC_DEALLOCATE:
    case RPC_INVALIDATE:
    case RPC_HAS_METHOD:
    case RPC_INVOKE:
    case RPC_INVOKE_DEFAULT:
    case RPC_HAS_PROPERTY:
    case RPC_GET_PROPERTY:
    case RPC_SET_PROPERTY:
    case RPC_REMOVE_PROPERTY:
    case RPC_ENUMERATION:
    case RPC_CONSTRUCT: {
      NPObjectStub* stub = GetStub(*capability);
      if (stub != NULL) {
         return stub->Dispatch(request, len);
      }
      break;
    }
    default:
      break;
  }
  return -1;
}

int NPBridge::Respond(const RpcHeader* reply, IOVec* iov, size_t iov_length) {
  assert(1 <= iov_length);
  assert(channel_ != kInvalidHtpHandle);
  assert(iov[0].base == reply);

  HtpHeader message;
  message.iov = iov;
  message.iov_length = iov_length;
  message.handles = handles_;
  message.handle_count = handle_count_;
  return SendDatagram(channel_, &message, 0);
}

}  // namespace nacl
