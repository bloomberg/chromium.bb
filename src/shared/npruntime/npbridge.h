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
struct NPCapability;

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
  // The process ID of the remote peer.
  int peer_pid_;
  // The size of NPVariant in the remote peer.
  size_t peer_npvariant_size_;

  // The map of NPObject to NPObjectStub
  std::map<NPObject*, NPObjectStub*> stub_map_;
  // The map of NPCapability to NPObjectProxy
  std::map<const NPCapability, NPObjectProxy*> proxy_map_;

#if NACL_BUILD_SUBARCH == 64
  // The map used for 64-bit plugins to convert NPP pointers to integers for
  // use by 32-bit NaCl modules.
  static std::map<NPP, int>* npp_64_32_map;
  // Convert back from integer to NPP.
  static std::map<int, NPP>* npp_32_64_map;

  // The map used for 64-bit plugins to convert NPIdentifiers to integers for
  // use by 32-bit NaCl modules.
  static std::map<NPIdentifier, int>* npident_int_map;
  // Convert back from integer to NPIdentifier.
  static std::map<int, NPIdentifier>* int_npident_map;
#endif  // NACL_BUILD_SUBARCH == 64

 private:
  // Adds the specified proxy to the map of NPCapability to NPObjectProxy
  void AddProxy(NPObjectProxy* proxy);

 protected:
  // The srpc channel to the remote peer.
  NaClSrpcChannel* channel_;

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
    return reinterpret_cast<NPBridge*>(
        (reinterpret_cast<NPInstance*>(npp->pdata))->module());
#endif
  }

  // Find the bridge corresponding to a given NaClSrpcChannel.
  static inline NPBridge* LookupBridge(NaClSrpcChannel* channel) {
    return reinterpret_cast<NPBridge*>(channel->server_instance_data);
  }

  // Converting NPPs to/from integers
  static int NppToInt(NPP npp);
  static NPP IntToNpp(int npp_int);

  // Converting NPIdentifiers to/from integers
  static int NpidentifierToInt(NPIdentifier npident);
  static NPIdentifier IntToNpidentifier(int npident_int);

  int peer_pid() const {
    return peer_pid_;
  }
  void set_peer_pid(int pid) {
    peer_pid_ = pid;
  }

  size_t peer_npvariant_size() const {
    return peer_npvariant_size_;
  }
  void set_peer_npvariant_size(size_t size) {
    peer_npvariant_size_ = size;
  }

  NaClSrpcChannel* channel() const {
    return channel_;
  }
  void set_channel(NaClSrpcChannel* channel) {
    channel_ = channel;
  }

  // Creates an NPObject stub for the specified object. On success,
  // CreateStub() returns 1, and NPCapability for the stub is filled in.
  // CreateStub() returns 0 on failure.
  int CreateStub(NPP npp, NPObject* object, NPCapability* cap);

  // Returns the NPObject stub that corresponds to the specified object.
  // LookupStub() returns NULL if no correspoing stub is found.
  NPObjectStub* LookupStub(NPObject* object);

  // Returns the NPObject stub that corresponds to the specified capability.
  // GetStub() returns NULL if no correspoing stub is found.
  NPObjectStub* GetStub(const NPCapability& capability);

  // Removes the specified NPObject stub.
  void RemoveStub(NPObjectStub* stub);

  // Creates a proxy NPObject for the specified capability. CreateProxy()
  // returns pointer to the proxy NPObject on success, and NULL on failure.
  NPObject* CreateProxy(NPP npp, const NPCapability& capability);

  // Returns the NPObject proxy that corresponds to the specified capability.
  // LookupProxy() returns NULL if no correspoing stub is found.
  NPObjectProxy* LookupProxy(const NPCapability& capability);

  // Removes the specified proxy to the map of NPCapability to NPObjectProxy
  void RemoveProxy(NPObjectProxy* proxy);
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPBRIDGE_H_
