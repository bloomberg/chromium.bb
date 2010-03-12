/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PLUGIN_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PLUGIN_H_

#include <stdio.h>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

#include "native_client/src/trusted/desc/nrd_all_modules.h"

// An incomplete class for the client runtime.
namespace nacl_srpc {

// Incomplete classes for method declarations.
class SocketAddress;
class ConnectedSocket;
class ServiceRuntimeInterface;
class SharedMemory;
template <typename HandleType>
class ScriptableHandle;


// The main plugin class.
// This represents scriptable objects returned by the plugin.  The ActiveX and
// NPAPI versions of the plugin instantiate subclasses of this class.
class Plugin : public PortableHandle {
 public:
  const char* local_url() { return local_url_; }

  void set_local_url(const char*);

  // Load from the local URL saved in local_url.
  // Saves local_url in local_url_ and origin in nacl_module_origin_.
  bool Load(nacl::string remote_url, const char* local_url);
  // Load nexe binary from the provided buffer.
  // Saves local_url in local_url_ and origin in nacl_module_origin_.
  bool Load(nacl::string remote_url,
            const char* local_url,
            const void* buffer,
            int32_t size);

  // Log a message by sending it to the service runtime.
  bool LogAtServiceRuntime(int severity, nacl::string msg);

  // Origin of page with the embed tag that created this plugin instance.
  nacl::string origin() { return origin_; }

  // Initialize some plugin data.
  bool Start();

  // Origin of NaCl module
  nacl::string nacl_module_origin() { return nacl_module_origin_; }
  void set_nacl_module_origin(nacl::string origin) {
    nacl_module_origin_ = origin;
  }

  // Constructor and destructor (for derived class)
  explicit Plugin();
  virtual ~Plugin();

  bool Init(struct PortableHandleInitializer* init_info);

  nacl::DescWrapperFactory* wrapper_factory() { return wrapper_factory_; }
  void LoadMethods();
  // overriding virtual methods
  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);
  virtual Plugin* GetPlugin();

  static int number_alive() { return number_alive_counter; }
  int32_t width();
  int32_t height();

 private:
  // Functions exported through MethodMap
  static bool UrlAsNaClDesc(void* obj, SrpcParams* params);
  static bool ShmFactory(void* obj, SrpcParams* params);
  static bool DefaultSocketAddress(void *obj, SrpcParams *params);
  static bool NullPluginMethod(void *obj, SrpcParams *params);
  static bool SocketAddressFactory(void* obj, SrpcParams* params);

  static bool GetHeightProperty(void* obj, SrpcParams* params);
  static bool SetHeightProperty(void* obj, SrpcParams* params);

  static bool GetModuleReadyProperty(void* obj, SrpcParams* params);
  static bool SetModuleReadyProperty(void* obj, SrpcParams* params);

  static bool GetPrintProperty(void* obj, SrpcParams* params);
  static bool SetPrintProperty(void* obj, SrpcParams* params);

  static bool GetSrcProperty(void* obj, SrpcParams* params);
  static bool SetSrcProperty(void* obj, SrpcParams* params);

  static bool GetVideoUpdateModeProperty(void* obj, SrpcParams* params);
  static bool SetVideoUpdateModeProperty(void* obj, SrpcParams* params);

  static bool GetWidthProperty(void* obj, SrpcParams* params);
  static bool SetWidthProperty(void* obj, SrpcParams* params);

  // Catch bad accesses during loading.
  static void SignalHandler(int value);

 private:
  static int number_alive_counter;
  ScriptableHandle<SocketAddress>* socket_address_;
  ScriptableHandle<ConnectedSocket>* socket_;
  ServiceRuntimeInterface* service_runtime_interface_;
  char* local_url_;
  int32_t height_;
  int32_t video_update_mode_;
  int32_t width_;

  nacl::string origin_;
  nacl::string nacl_module_origin_;

  bool origin_valid_;

  nacl::DescWrapperFactory* wrapper_factory_;

  static PLUGIN_JMPBUF loader_env;

  static bool identifiers_initialized;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PLUGIN_H_
