// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_URL_LOADER_PROXY_H_
#define PPAPI_PPB_URL_LOADER_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_URLLoader_Dev;

namespace pp {
namespace proxy {

class PPB_URLLoader_Proxy : public InterfaceProxy {
 public:
  PPB_URLLoader_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_URLLoader_Proxy();

  const PPB_URLLoader_Dev* ppb_url_loader_target() const {
    return reinterpret_cast<const PPB_URLLoader_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  // Plugin->renderer message handlers.
  void OnMsgCreate(PP_Instance instance,
                   PP_Resource* result);
  void OnMsgOpen(PP_Resource loader,
                 PP_Resource request_info,
                 uint32_t serialized_callback);
  void OnMsgFollowRedirect(PP_Resource loader,
                           uint32_t serialized_callback);
  void OnMsgGetUploadProgress(PP_Resource loader,
                              int64* bytes_sent,
                              int64* total_bytes_to_be_sent,
                              bool* result);
  void OnMsgGetDownloadProgress(PP_Resource loader,
                                int64* bytes_received,
                                int64* total_bytes_to_be_received,
                                bool* result);
  void OnMsgGetResponseInfo(PP_Resource loader,
                            PP_Resource* result);
  void OnMsgReadResponseBody(PP_Resource loader,
                             int32_t bytes_to_read,
                             uint32_t serialized_callback);
  void OnMsgFinishStreamingToFile(PP_Resource loader,
                                  uint32_t serialized_callback);
  void OnMsgClose(PP_Resource loader);

  // Renderer->plugin message handlers.
  void OnMsgUpdateProgress(PP_Resource resource,
                           int64_t bytes_sent,
                           int64_t total_bytes_to_be_sent,
                           int64_t bytes_received,
                           int64_t total_bytes_to_be_received);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_URL_LOADER_PROXY_H_
