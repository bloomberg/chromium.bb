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
  PPB_Graphics2D_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Graphics2D_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         const PP_Size& size,
                                         PP_Bool is_always_opaque);

  const PPB_Graphics2D* ppb_graphics_2d_target() const {
    return static_cast<const PPB_Graphics2D*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Plugin->renderer message handlers.
  void OnMsgPaintImageData(const ppapi::HostResource& graphics_2d,
                           const ppapi::HostResource& image_data,
                           const PP_Point& top_left,
                           bool src_rect_specified,
                           const PP_Rect& src_rect);
  void OnMsgScroll(const ppapi::HostResource& graphics_2d,
                   bool clip_specified,
                   const PP_Rect& clip,
                   const PP_Point& amount);
  void OnMsgReplaceContents(const ppapi::HostResource& graphics_2d,
                            const ppapi::HostResource& image_data);
  void OnMsgFlush(const ppapi::HostResource& graphics_2d);

  // Renderer->plugin message handlers.
  void OnMsgFlushACK(const ppapi::HostResource& graphics_2d,
                     int32_t pp_error);

  // Called in the renderer to send the given flush ACK to the plugin.
  void SendFlushACKToPlugin(int32_t result,
                            const ppapi::HostResource& graphics_2d);

  pp::CompletionCallbackFactory<PPB_Graphics2D_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_GRAPHICS_2D_PROXY_H_
