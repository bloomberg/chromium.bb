// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_

#include <assert.h>
#include <string.h>
#include <time.h>

#include <functional>
#include <set>
#include <map>

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/npinstance.h"

namespace nacl {
// Incomplete class declarations.
class NPObjectStub;
class NPObjectProxy;
class NPCapability;

// NPBridge provides two main services.
// 1) It encapsulates one end of a connection between a NaCl NPAPI module
//    and a browser plugin.
// 2) It manages NPObject proxies and stubs between two processes.
//
// The NPNavigator class extends NPBridge to represent the NaCl module end of
// the connection.  There can be only one NPNavigator per NaCl module.
//
// The NPModule class extends the NPBridge class to represent the
// plugin end of the connection to the NaCl module.  There can be only one
// NPModule attached to a given NaCl module.  However, for example, there can
// be more than one instance of a NaCl module in one page (more than one embed,
// for example), or more than one page in a renderer.  Hence, there can be
// more than one NPModule instance.
class NPBridge {
 public:
  // The maximum number of arguments passed from the plugin to the
  // NPP_New in the NaCl module.
  static const uint32_t kMaxArgc = 64;
  static const uint32_t kMaxArgLength = 256;

  explicit NPBridge();
  virtual ~NPBridge();

  // Find the bridge corresponding to a given NPP.
  static inline NPBridge* LookupBridge(NPP npp) {
#ifdef __native_client__
    // For a NaCl module the bridge (NPNavigator) is pointed to by npp->ndata
    return reinterpret_cast<NPBridge*>(npp->ndata);
#else
    // For the browser the bridge (NPModule) is retained on the NPInstance
    // retained by npp->pdata.
    if (NULL == npp->pdata) {
      return NULL;
    }
    return reinterpret_cast<NPBridge*>(
        (reinterpret_cast<NPInstance*>(npp->pdata))->module());
#endif
  }

  // Find the bridge corresponding to a given NaClSrpcChannel.
  static inline NPBridge* LookupBridge(NaClSrpcChannel* channel) {
    return reinterpret_cast<NPBridge*>(channel->server_instance_data);
  }

  int peer_pid() const {
    return peer_pid_;
  }
  void set_peer_pid(int pid) {
    peer_pid_ = pid;
  }

  NaClSrpcChannel* channel() const {
    return channel_;
  }
  void set_channel(NaClSrpcChannel* channel) {
    channel_ = channel;
  }

  // Creates a proxy NPObject for the specified capability. CreateProxy()
  // returns pointer to the proxy NPObject on success, and NULL on failure.
  NPObject* CreateProxy(NPP npp, const NPCapability& capability);

  // Returns the NPObject proxy that corresponds to the specified capability.
  // LookupProxy() returns NULL if no correspoing stub is found.
  NPObjectProxy* LookupProxy(const NPCapability& capability);

  // Removes the specified proxy from the map of NPCapability to NPObjectProxy
  void RemoveProxy(NPObjectProxy* proxy);

 protected:
  // The srpc channel to the remote peer.
  NaClSrpcChannel* channel_;

 private:
  // Adds the specified proxy to the map of NPCapability to NPObjectProxy
  void AddProxy(NPObjectProxy* proxy);

  // The process ID of the remote peer.
  int peer_pid_;

  // The map of NPCapability to NPObjectProxy
  std::map<const NPCapability, NPObjectProxy*> proxy_map_;

  // The number of bridge instances currently alive.  This is used to
  // determine when to free the WireFormat translation data structures.
  static int number_bridges_alive;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_
