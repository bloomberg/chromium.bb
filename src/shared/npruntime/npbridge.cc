// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#include "native_client/src/shared/npruntime/npbridge.h"

#include <assert.h>
#include <errno.h>
#ifdef __native_client__
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {

int NPBridge::number_bridges_alive = 0;

NPBridge::NPBridge()
    : channel_(NULL),
      peer_pid_(-1),
      peer_npvariant_size_(0) {
  // Set up the translations for NPP, NPIdentifier, etc.
  if (0 == number_bridges_alive) {
    WireFormatInit();
  }
  ++number_bridges_alive;
  // Set up the proxy lookup table.
  NPObjectStub::AddBridge();
}

NPBridge::~NPBridge() {
  // TODO(sehr): parameter should be set to true unless NPN_Invalidate was
  // called.
  NPObjectStub::RemoveBridge(false);

  --number_bridges_alive;
  if (0 == number_bridges_alive) {
    // Delete the translations for NPP, NPIdentifier, etc.
    WireFormatFini();
  }
}

NPObject* NPBridge::CreateProxy(NPP npp, const NPCapability& capability) {
  NPObject* object = capability.object();
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
  printf("LookupProxy(%p): %p %"NACL_PRId64"\n",
         reinterpret_cast<const void*>(&capability),
         reinterpret_cast<void*>(capability.object()),
         capability.pid());
  if (NULL == capability.object()) {
    return NULL;
  }
  if (GETPID() == capability.pid()) {
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
