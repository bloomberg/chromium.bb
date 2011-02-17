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

struct PPB_FileChooser_Dev;

namespace pp {
namespace proxy {

class HostResource;
struct PPBFileRef_CreateInfo;

class PPB_FileChooser_Proxy : public InterfaceProxy {
 public:
  PPB_FileChooser_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_FileChooser_Proxy();

  static const Info* GetInfo();

  const PPB_FileChooser_Dev* ppb_file_chooser_target() const {
    return static_cast<const PPB_FileChooser_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Plugin -> host message handlers.
  void OnMsgCreate(PP_Instance instance,
                   int mode,
                   const std::string& accept_mime_types,
                   pp::proxy::HostResource* result);
  void OnMsgShow(const pp::proxy::HostResource& chooser);

  // Host -> plugin message handlers.
  void OnMsgChooseComplete(
      const pp::proxy::HostResource& chooser,
      int32_t result_code,
      const std::vector<PPBFileRef_CreateInfo>& chosen_files);

  // Called when the show is complete in the host. This will notify the plugin
  // via IPC and OnMsgChooseComplete will be called there.
  void OnShowCallback(int32_t result, const HostResource& chooser);

  CompletionCallbackFactory<PPB_FileChooser_Proxy,
                            ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileChooser_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_FILE_CHOOSER_PROXY_H_
