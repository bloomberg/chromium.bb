// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FILE_REF_PROXY_H_
#define PPAPI_PROXY_PPB_FILE_REF_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace ppapi {

struct PPB_FileRef_CreateInfo;

namespace proxy {

class SerializedVarReturnValue;

class PPAPI_PROXY_EXPORT PPB_FileRef_Proxy
    : public NON_EXPORTED_BASE(InterfaceProxy) {
 public:
  explicit PPB_FileRef_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_FileRef_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         PP_Resource file_system,
                                         const char* path);
  static PP_Resource CreateProxyResource(
      const PPB_FileRef_CreateInfo& serialized);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Takes a resource in the host and converts it into a serialized file ref
  // "create info" for reconstitution in the plugin. This struct contains all
  // the necessary information about the file ref.
  //
  // Various PPAPI functions return file refs from various interfaces, so this
  // function is public so anybody can send a file ref.
  static void SerializeFileRef(PP_Resource file_ref,
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
  // Plugin -> host message handlers.
  void OnMsgCreate(PP_Instance instance,
                   PP_Resource file_system,
                   const std::string& path,
                   PPB_FileRef_CreateInfo* result);
  void OnMsgGetParent(const HostResource& host_resource,
                      PPB_FileRef_CreateInfo* result);
  void OnMsgMakeDirectory(const HostResource& host_resource,
                          PP_Bool make_ancestors,
                          uint32_t callback_id);
  void OnMsgTouch(const HostResource& host_resource,
                  PP_Time last_access,
                  PP_Time last_modified,
                  uint32_t callback_id);
  void OnMsgDelete(const HostResource& host_resource,
                   uint32_t callback_id);
  void OnMsgRename(const HostResource& file_ref,
                   const HostResource& new_file_ref,
                   uint32_t callback_id);
  void OnMsgQuery(const HostResource& file_ref,
                  uint32_t callback_id);
  void OnMsgGetAbsolutePath(const HostResource& host_resource,
                            SerializedVarReturnValue result);
  void OnMsgReadDirectoryEntries(const HostResource& file_ref,
                                 uint32_t callback_id);

  // Host -> Plugin message handlers.
  void OnMsgCallbackComplete(const HostResource& host_resource,
                             uint32_t callback_id,
                             int32_t result);
  void OnMsgQueryCallbackComplete(const HostResource& host_resource,
                                  const PP_FileInfo& info,
                                  uint32_t callback_id,
                                  int32_t result);
  void OnMsgReadDirectoryEntriesCallbackComplete(
      const HostResource& host_resource,
      const std::vector<ppapi::PPB_FileRef_CreateInfo>& infos,
      const std::vector<PP_FileType>& file_types,
      uint32_t callback_id,
      int32_t result);

  struct HostCallbackParams {
    HostCallbackParams(const HostResource& host_res, uint32_t cb_id)
        : host_resource(host_res), callback_id(cb_id) {
    }
    HostResource host_resource;
    uint32_t callback_id;
  };

  void OnCallbackCompleteInHost(int32_t result,
                                const HostResource& host_resource,
                                uint32_t callback_id);
  void OnQueryCallbackCompleteInHost(
      int32_t result,
      const HostResource& host_resource,
      linked_ptr<PP_FileInfo> info,
      uint32_t callback_id);
  void OnReadDirectoryEntriesCallbackCompleteInHost(
      int32_t result,
      HostCallbackParams params,
      linked_ptr<std::vector<ppapi::PPB_FileRef_CreateInfo> > files,
      linked_ptr<std::vector<PP_FileType> > file_types);

  ProxyCompletionCallbackFactory<PPB_FileRef_Proxy> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileRef_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FILE_REF_PROXY_H_
