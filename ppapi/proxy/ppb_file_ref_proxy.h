// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FILE_REF_PROXY_H_
#define PPAPI_PROXY_PPB_FILE_REF_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

namespace ppapi {

class HostResource;
struct PPB_FileRef_CreateInfo;

namespace proxy {

class SerializedVarReturnValue;

class PPB_FileRef_Proxy : public InterfaceProxy {
 public:
  explicit PPB_FileRef_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_FileRef_Proxy();

  static PP_Resource CreateProxyResource(PP_Resource file_system,
                                         const char* path);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Takes a resource in the host and converts it into a serialized file ref
  // "create info" for reconstitution in the plugin. This struct contains all
  // the necessary information about the file ref.
  //
  // This function is not static because it needs access to the particular
  // dispatcher and host interface.
  //
  // Various PPAPI functions return file refs from various interfaces, so this
  // function is public so anybody can send a file ref.
  void SerializeFileRef(PP_Resource file_ref,
                        PPB_FileRef_CreateInfo* result);

  // Creates a plugin resource from the given CreateInfo sent from the host.
  // The value will be the result of calling SerializeFileRef on the host.
  // This represents passing the resource ownership to the plugin. This
  // function also checks the validity of the result and returns 0 on failure.
  //
  // Various PPAPI functions return file refs from various interfaces, so this
  // function is public so anybody can receive a file ref.
  static PP_Resource DeserializeFileRef(
      const PPB_FileRef_CreateInfo& serialized);

  static const ApiID kApiID = API_ID_PPB_FILE_REF;

 private:
  // Message handlers.
  void OnMsgCreate(const HostResource& file_system,
                   const std::string& path,
                   PPB_FileRef_CreateInfo* result);
  void OnMsgGetParent(const HostResource& host_resource,
                      PPB_FileRef_CreateInfo* result);
  void OnMsgMakeDirectory(const HostResource& host_resource,
                          PP_Bool make_ancestors,
                          int callback_id);
  void OnMsgTouch(const HostResource& host_resource,
                  PP_Time last_access,
                  PP_Time last_modified,
                  int callback_id);
  void OnMsgDelete(const HostResource& host_resource,
                   int callback_id);
  void OnMsgRename(const HostResource& file_ref,
                   const HostResource& new_file_ref,
                   int callback_id);
  void OnMsgGetAbsolutePath(const HostResource& host_resource,
                            SerializedVarReturnValue result);

  // Host -> Plugin message handlers.
  void OnMsgCallbackComplete(const HostResource& host_resource,
                             int callback_id,
                             int32_t result);

  void OnCallbackCompleteInHost(int32_t result,
                                const HostResource& host_resource,
                                int callback_id);

  pp::CompletionCallbackFactory<PPB_FileRef_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileRef_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FILE_REF_PROXY_H_
