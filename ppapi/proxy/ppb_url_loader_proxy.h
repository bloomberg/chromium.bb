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
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_URLLoader;
struct PPB_URLLoaderTrusted;

namespace pp {
namespace proxy {

class PPB_URLLoader_Proxy : public InterfaceProxy {
 public:
  PPB_URLLoader_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_URLLoader_Proxy();

  // URLLoader objects are normally allocated by the Create function, but
  // they are also provided to PPP_Instance.OnMsgHandleDocumentLoad. This
  // function allows the proxy for DocumentLoad to create the correct plugin
  // proxied info for the given browser-supplied URLLoader resource ID.
  static void TrackPluginResource(PP_Instance instance,
                                  PP_Resource url_loader_resource);

  const PPB_URLLoader* ppb_url_loader_target() const {
    return reinterpret_cast<const PPB_URLLoader*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Data associated with callbacks for ReadResponseBody.
  struct ReadCallbackInfo;

  // Plugin->renderer message handlers.
  void OnMsgCreate(PP_Instance instance,
                   PP_Resource* result);
  void OnMsgOpen(PP_Resource loader,
                 PP_Resource request_info,
                 uint32_t serialized_callback);
  void OnMsgFollowRedirect(PP_Resource loader,
                           uint32_t serialized_callback);
  void OnMsgGetResponseInfo(PP_Resource loader,
                            PP_Resource* result);
  void OnMsgReadResponseBody(PP_Resource loader,
                             int32_t bytes_to_read);
  void OnMsgFinishStreamingToFile(PP_Resource loader,
                                  uint32_t serialized_callback);
  void OnMsgClose(PP_Resource loader);

  // Renderer->plugin message handlers.
  void OnMsgUpdateProgress(PP_Resource resource,
                           int64_t bytes_sent,
                           int64_t total_bytes_to_be_sent,
                           int64_t bytes_received,
                           int64_t total_bytes_to_be_received);
  void OnMsgReadResponseBodyAck(PP_Resource pp_resource,
                                int32_t result,
                                const std::string& data);

  // Handles callbacks for read complete messages. Takes ownership of the info
  // pointer.
  void OnReadCallback(int32_t result, ReadCallbackInfo* info);

  CompletionCallbackFactory<PPB_URLLoader_Proxy,
                            ProxyNonThreadSafeRefCount> callback_factory_;
};

class PPB_URLLoaderTrusted_Proxy : public InterfaceProxy {
 public:
  PPB_URLLoaderTrusted_Proxy(Dispatcher* dispatcher,
                             const void* target_interface);
  virtual ~PPB_URLLoaderTrusted_Proxy();

  const PPB_URLLoaderTrusted* ppb_url_loader_trusted_target() const {
    return reinterpret_cast<const PPB_URLLoaderTrusted*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Plugin->renderer message handlers.
  void OnMsgGrantUniversalAccess(PP_Resource loader);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_URL_LOADER_PROXY_H_
