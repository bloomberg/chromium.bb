/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CONNECTED_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CONNECTED_SOCKET_H_

// TODO(gregoryd): reduce the headers needed for this.
#include <setjmp.h>
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl_srpc {

// Forward declarations for externals.
class Plugin;
class ServiceRuntimeInterface;
class SharedMemory;
class SrpcClient;
struct DescHandleInitializer;
class DescBasedHandle;

struct ConnectedSocketInitializer: DescHandleInitializer {
  bool is_srpc_client_;
  ServiceRuntimeInterface* serv_rtm_info_;
  bool is_command_channel_;
  ConnectedSocketInitializer(PortablePluginInterface* plugin_interface,
                             nacl::DescWrapper* desc,
                             Plugin* plugin,
                             bool is_srpc_client,
                             ServiceRuntimeInterface* serv_rtm_info):
      DescHandleInitializer(plugin_interface, desc, plugin),
      is_srpc_client_(is_srpc_client),
      serv_rtm_info_(serv_rtm_info),
      is_command_channel_(NULL == serv_rtm_info) {}
};


// ConnectedSocket represents a connected socket that results from loading
// a NativeClient module or doing a connect on a received descriptor
// (SocketAddress).
class ConnectedSocket : public DescBasedHandle {
 public:
  bool Init(struct PortableHandleInitializer* init_info);
  void LoadMethods();
  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);

  explicit ConnectedSocket();
  // ServiceRuntimeInterface instances need to delete an instance.
  ~ConnectedSocket();

  static int number_alive() { return number_alive_counter; }

 private:
  static void SignalHandler(int value);
  NaClSrpcArg* GetSignatureObject();

  // registered methods
  static bool SignaturesGetProperty(void* obj, SrpcParams* params);

 private:
  static int number_alive_counter;
  NaClSrpcArg* signatures_;
  ServiceRuntimeInterface* service_runtime_info_;
  SrpcClient* srpc_client_;
  static PLUGIN_JMPBUF socket_env;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CONNECTED_SOCKET_H_
