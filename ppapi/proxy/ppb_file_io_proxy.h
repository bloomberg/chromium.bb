// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FILE_IO_PROXY_H_
#define PPAPI_PROXY_PPB_FILE_IO_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace ppapi {

class HostResource;
namespace proxy {

class PPB_FileIO_Proxy : public InterfaceProxy {
 public:
  explicit PPB_FileIO_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_FileIO_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_FILE_IO;

 private:
  // Plugin -> Host message handlers.
  void OnHostMsgCreate(PP_Instance instance, HostResource* result);
  void OnHostMsgOpen(const HostResource& host_resource,
                     const HostResource& file_ref_resource,
                     int32_t open_flags);
  void OnHostMsgClose(const HostResource& host_resource);
  void OnHostMsgQuery(const HostResource& host_resource);
  void OnHostMsgTouch(const HostResource& host_resource,
                      PP_Time last_access_time,
                      PP_Time last_modified_time);
  void OnHostMsgRead(const HostResource& host_resource,
                     int64_t offset,
                     int32_t bytes_to_read);
  void OnHostMsgWrite(const HostResource& host_resource,
                      int64_t offset,
                      const std::string& data);
  void OnHostMsgSetLength(const HostResource& host_resource,
                          int64_t length);
  void OnHostMsgFlush(const HostResource& host_resource);
  void OnHostMsgWillWrite(const HostResource& host_resource,
                          int64_t offset,
                          int32_t bytes_to_write);
  void OnHostMsgWillSetLength(const HostResource& host_resource,
                              int64_t length);

  // Host -> Plugin message handlers.
  void OnPluginMsgGeneralComplete(const HostResource& host_resource,
                                  int32_t result);
  void OnPluginMsgOpenFileComplete(const HostResource& host_resource,
                                  int32_t result);
  void OnPluginMsgQueryComplete(const HostResource& host_resource,
                                int32_t result,
                                const PP_FileInfo& info);
  void OnPluginMsgReadComplete(const HostResource& host_resource,
                               int32_t result,
                               const std::string& data);
  void OnPluginMsgWriteComplete(const HostResource& host_resource,
                                int32_t result);

  // Host-side callback handlers. These convert the callbacks to an IPC message
  // to the plugin.
  void GeneralCallbackCompleteInHost(int32_t pp_error,
                                     const HostResource& host_resource);
  void OpenFileCallbackCompleteInHost(int32_t pp_error,
                                      const HostResource& host_resource);
  void QueryCallbackCompleteInHost(int32_t pp_error,
                                   const HostResource& host_resource,
                                   PP_FileInfo* info);
  void ReadCallbackCompleteInHost(int32_t pp_error,
                                  const HostResource& host_resource,
                                  std::string* data);
  ProxyCompletionCallbackFactory<PPB_FileIO_Proxy> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileIO_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FILE_IO_PROXY_H_
