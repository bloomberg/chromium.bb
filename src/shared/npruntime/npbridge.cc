// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#if NACL_BUILD_SUBARCH == 64
std::map<NPP, int>* NPBridge::npp_64_32_map = NULL;
std::map<int, NPP>* NPBridge::npp_32_64_map = NULL;
std::map<NPIdentifier, int>* NPBridge::npident_int_map = NULL;
std::map<int, NPIdentifier>* NPBridge::int_npident_map = NULL;
#endif  // NACL_BUILD_SUBARCH

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
  NPObjectStub::AddBridge();
}

NPBridge::~NPBridge() {
  // TODO(sehr): parameter should be set to true unless NPN_Invalidate was
  // called.
  NPObjectStub::RemoveBridge(false);
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
  NPObject* object = capability.object;
  if (NULL == object) {
    // Do not create proxies for NULL objects.
    return NULL;
  }
  if (NULL != NPObjectStub::GetByCapability(capability)) {
    // Found the object in the stub table, so the capability was previously
    // given out.  Hence it is ok to use the object.  Bump the refcount and
    // return the object.
    return NPN_RetainObject(object);
  }
  // The capability is to an object in another process.
  std::map<const NPCapability, NPObjectProxy*>::iterator i;
  i = proxy_map_.find(capability);
  if (proxy_map_.end() != i) {
    // Found the proxy.  Bump the reference count and return it.
    return NPN_RetainObject((*i).second);
  }
  // Create a new proxy.
  NPObjectProxy* proxy = new(std::nothrow) NPObjectProxy(npp, capability);
  if (NULL == proxy) {
    // Out of memory.
    return NULL;
  }
  // Add it to the local lookup table.
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

}  // namespace nacl
