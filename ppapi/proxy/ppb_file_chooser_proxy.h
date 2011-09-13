// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FILE_CHOOSER_PROXY_H_
#define PPAPI_PROXY_PPB_FILE_CHOOSER_PROXY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"

struct PPB_FileChooser_Dev;

namespace ppapi {

class HostResource;
struct PPB_FileRef_CreateInfo;

namespace proxy {

class SerializedVarReceiveInput;

class PPB_FileChooser_Proxy : public InterfaceProxy {
 public:
  PPB_FileChooser_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_FileChooser_Proxy();

  static PP_Resource CreateProxyResource(
      PP_Instance instance,
      PP_FileChooserMode_Dev mode,
      const PP_Var& accept_mime_types);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const InterfaceID kInterfaceID = INTERFACE_ID_PPB_FILE_CHOOSER;

 private:
  // Plugin -> host message handlers.
  void OnMsgCreate(PP_Instance instance,
                   int mode,
                   SerializedVarReceiveInput accept_mime_types,
                   ppapi::HostResource* result);
  void OnMsgShow(const ppapi::HostResource& chooser);

  // Host -> plugin message handlers.
  void OnMsgChooseComplete(
      const ppapi::HostResource& chooser,
      int32_t result_code,
      const std::vector<PPB_FileRef_CreateInfo>& chosen_files);

  // Called when the show is complete in the host. This will notify the plugin
  // via IPC and OnMsgChooseComplete will be called there.
  void OnShowCallback(int32_t result, const ppapi::HostResource& chooser);

  pp::CompletionCallbackFactory<PPB_FileChooser_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileChooser_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FILE_CHOOSER_PROXY_H_
