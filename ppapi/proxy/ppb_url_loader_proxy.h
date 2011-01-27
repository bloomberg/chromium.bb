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
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_URLLoader;
struct PPB_URLLoaderTrusted;

namespace pp {
namespace proxy {

struct PPBURLLoader_UpdateProgress_Params;

class PPB_URLLoader_Proxy : public InterfaceProxy {
 public:
  PPB_URLLoader_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_URLLoader_Proxy();

  // URLLoader objects are normally allocated by the Create function, but
  // they are also provided to PPP_Instance.OnMsgHandleDocumentLoad. This
  // function allows the proxy for DocumentLoad to create the correct plugin
  // proxied info for the given browser-supplied URLLoader resource ID.
  static PP_Resource TrackPluginResource(
      const HostResource& url_loader_resource);

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
                   HostResource* result);
  void OnMsgOpen(const HostResource& loader,
                 const HostResource& request_info,
                 uint32_t serialized_callback);
  void OnMsgFollowRedirect(const HostResource& loader,
                           uint32_t serialized_callback);
  void OnMsgGetResponseInfo(const HostResource& loader,
                            HostResource* result);
  void OnMsgReadResponseBody(const HostResource& loader,
                             int32_t bytes_to_read);
  void OnMsgFinishStreamingToFile(const HostResource& loader,
                                  uint32_t serialized_callback);
  void OnMsgClose(const HostResource& loader);

  // Renderer->plugin message handlers.
  void OnMsgUpdateProgress(
      const PPBURLLoader_UpdateProgress_Params& params);
  void OnMsgReadResponseBodyAck(const HostResource& pp_resource,
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
  void OnMsgGrantUniversalAccess(const HostResource& loader);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_URL_LOADER_PROXY_H_
