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

#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {

#if NACL_TARGET_SUBARCH == 64
std::map<NPP, int>* NPBridge::npp_64_32_map = NULL;
std::map<int, NPP>* NPBridge::npp_32_64_map = NULL;
std::map<NPIdentifier, int>* NPBridge::npident_int_map = NULL;
std::map<int, NPIdentifier>* NPBridge::int_npident_map = NULL;
#endif  // NACL_TARGET_SUBARCH

NPBridge::NPBridge()
    : peer_pid_(-1),
      peer_npvariant_size_(0),  // Must be set later
      channel_(NULL) {
#if NACL_BUILD_SUBARCH == 64
  // The mappings from NPP to integers and back for interchange with NaCl
  // modules
  if (NULL == npp_64_32_map || NULL == npp_32_64_map) {
    npp_64_32_map = new(std::nothrow) std::map<NPP, int>;
    npp_32_64_map = new(std::nothrow) std::map<int, NPP>;
  }
  if (NULL == npident_int_map || NULL == int_npident_map) {
    npident_int_map = new(std::nothrow) std::map<NPIdentifier, int>;
    int_npident_map = new(std::nothrow) std::map<int, NPIdentifier>;
  }
#endif  // NACL_BUILD_SUBARCH
}

NPBridge::~NPBridge() {
  // TODO(sehr): the cleanup needs to be done when an instance shuts down.
  // Free the stubs created on the other end of the channel.
  while (!stub_map_.empty()) {
    std::map<NPObject*, NPObjectStub*>::iterator i = stub_map_.begin();
    NPObjectStub* stub = (*i).second;
    delete stub;
  }
}

int NPBridge::NppToInt(NPP npp) {
#if NACL_BUILD_SUBARCH == 64
  static int next_npp_int = 0;
  if (NULL == npp_64_32_map || NULL == npp_32_64_map) {
    // Translation called without proper setup.
    return -1;
  }
  if (npp_64_32_map->end() == npp_64_32_map->find(npp)) {
    if (-1 == next_npp_int) {
      // We have wrapped around and consumed all the identifiers.
      return -1;
    }
    (*npp_32_64_map)[next_npp_int] = npp;
    (*npp_64_32_map)[npp] = next_npp_int++;
  }
  return (*npp_64_32_map)[npp];
#else
  return reinterpret_cast<int>(npp);
#endif  // NACL_BUILD_SUBARCH
}

NPP NPBridge::IntToNpp(int npp_int) {
#if NACL_BUILD_SUBARCH == 64
  if (NULL == npp_64_32_map || NULL == npp_32_64_map) {
    // Translation called without proper setup.
    return NULL;
  }
  if (npp_32_64_map->end() == npp_32_64_map->find(npp_int)) {
    // No mapping was created for this value.
    return NULL;
  }
  return (*npp_32_64_map)[npp_int];
#else
  return reinterpret_cast<NPP>(npp_int);
#endif  // NACL_BUILD_SUBARCH
}

int NPBridge::NpidentifierToInt(NPIdentifier npident) {
#if NACL_BUILD_SUBARCH == 64
  static int next_npident_int = 0;
  if (NULL == npident_int_map || NULL == int_npident_map) {
    // Translation called without proper setup.
    return -1;
  }
  if (npident_int_map->end() == npident_int_map->find(npident)) {
    if (-1 == next_npident_int) {
      // We have wrapped around and consumed all the identifiers.
      return -1;
    }
    (*int_npident_map)[next_npident_int] = npident;
    (*npident_int_map)[npident] = next_npident_int++;
  }
  return (*npident_int_map)[npident];
#else
  return reinterpret_cast<int>(npident);
#endif  // NACL_BUILD_SUBARCH
}

NPIdentifier NPBridge::IntToNpidentifier(int npident_int) {
#if NACL_BUILD_SUBARCH == 64
  if (NULL == npident_int_map || NULL == int_npident_map) {
    // Translation called without proper setup.
    return NULL;
  }
  if (int_npident_map->end() == int_npident_map->find(npident_int)) {
    // No mapping was created for this value.
    return NULL;
  }
  return (*int_npident_map)[npident_int];
#else
  return reinterpret_cast<NPIdentifier>(npident_int);
#endif  // NACL_BUILD_SUBARCH
}

NPObject* NPBridge::CreateProxy(NPP npp, const NPCapability& capability) {
  if (NULL == capability.object) {
    return NULL;
  }
  if (capability.pid == GETPID()) {  // capability can be of my process
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
  if (proxy_map_.end() != i) {
    NPObjectProxy* proxy = (*i).second;
    NPN_RetainObject(proxy);
    return proxy;
  }
  NPObjectProxy* proxy = new(std::nothrow) NPObjectProxy(npp, capability);
  if (NULL == proxy) {
    return NULL;
  }
  AddProxy(proxy);
  return proxy;
}

NPObjectProxy* NPBridge::LookupProxy(const NPCapability& capability) {
  printf("LookupProxy(%p): %p %d\n",
         reinterpret_cast<const void*>(&capability),
         reinterpret_cast<void*>(capability.object),
         capability.pid);
  if (NULL == capability.object) {
    return NULL;
  }
  if (GETPID() == capability.pid) {
    return NULL;
  }
  std::map<const NPCapability, NPObjectProxy*>::iterator i;
  i = proxy_map_.find(capability);
  if (i != proxy_map_.end()) {
    return (*i).second;
  }
  return NULL;
}

void NPBridge::AddProxy(NPObjectProxy* proxy) {
  proxy_map_[proxy->capability()] = proxy;
}

void NPBridge::RemoveProxy(NPObjectProxy* proxy) {
  proxy_map_.erase(proxy->capability());
}

int NPBridge::CreateStub(NPP npp, NPObject* object, NPCapability* cap) {
  NPObjectStub* stub = NULL;
  if (NULL != object) {
    if (NPObjectProxy::IsInstance(object)) {  // object can be a proxy
      NPObjectProxy* proxy = static_cast<NPObjectProxy*>(object);
      if (peer_pid() == proxy->capability().pid) {
        *cap = proxy->capability();
        return 0;
      }
    }
    stub = LookupStub(object);
    if (stub) {
      cap->pid = GETPID();
      cap->object = object;
      return 1;
    }
    stub = new(std::nothrow) NPObjectStub(npp, object);
  }
  cap->pid = GETPID();
  if (NULL == stub) {
    cap->object = NULL;
    return 0;
  }
  stub_map_[object] = stub;
  cap->object = object;
  return 1;
}

NPObjectStub* NPBridge::LookupStub(NPObject* object) {
  assert(object);
  std::map<NPObject*, NPObjectStub*>::iterator i;
  i = stub_map_.find(object);
  if (stub_map_.end() != i) {
    return (*i).second;
  }
  return NULL;
}

NPObjectStub* NPBridge::GetStub(const NPCapability& capability) {
  if (GETPID() != capability.pid) {
    return NULL;
  }
  std::map<NPObject*, NPObjectStub*>::iterator i;
  i = stub_map_.find(capability.object);
  if (stub_map_.end() == i) {
    return NULL;
  }
  return (*i).second;
}

void NPBridge::RemoveStub(NPObjectStub* stub) {
  stub_map_.erase(stub->object());
}

}  // namespace nacl
