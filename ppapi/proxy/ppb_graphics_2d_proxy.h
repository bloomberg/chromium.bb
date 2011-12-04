// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_GRAPHICS_2D_PROXY_H_
#define PPAPI_PPB_GRAPHICS_2D_PROXY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_Graphics2D;
struct PP_Point;
struct PP_Rect;

namespace ppapi {
namespace proxy {

class PPB_Graphics2D_Proxy : public InterfaceProxy {
 public:
  PPB_Graphics2D_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Graphics2D_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         const PP_Size& size,
                                         PP_Bool is_always_opaque);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_GRAPHICS_2D;

 private:
  // Plugin->host message handlers.
  void OnHostMsgCreate(PP_Instance instance,
                       const PP_Size& size,
                       PP_Bool is_always_opaque,
                       HostResource* result);
  void OnHostMsgPaintImageData(const HostResource& graphics_2d,
                               const HostResource& image_data,
                               const PP_Point& top_left,
                               bool src_rect_specified,
                               const PP_Rect& src_rect);
  void OnHostMsgScroll(const HostResource& graphics_2d,
                       bool clip_specified,
                       const PP_Rect& clip,
                       const PP_Point& amount);
  void OnHostMsgReplaceContents(const HostResource& graphics_2d,
                                const HostResource& image_data);
  void OnHostMsgFlush(const HostResource& graphics_2d);

  // Host->plugin message handlers.
  void OnPluginMsgFlushACK(const HostResource& graphics_2d,
                           int32_t pp_error);

  // Called in the renderer to send the given flush ACK to the plugin.
  void SendFlushACKToPlugin(int32_t result,
                            const HostResource& graphics_2d);

  pp::CompletionCallbackFactory<PPB_Graphics2D_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics2D_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_GRAPHICS_2D_PROXY_H_
