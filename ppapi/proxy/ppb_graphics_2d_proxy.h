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
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"

struct PPB_Graphics2D;
struct PP_Point;
struct PP_Rect;

namespace pp {
namespace proxy {

class PPB_Graphics2D_Proxy : public InterfaceProxy {
 public:
  PPB_Graphics2D_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Graphics2D_Proxy();

  static const Info* GetInfo();

  const PPB_Graphics2D* ppb_graphics_2d_target() const {
    return static_cast<const PPB_Graphics2D*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Plugin->renderer message handlers.
  void OnMsgPaintImageData(const HostResource& graphics_2d,
                           const HostResource& image_data,
                           const PP_Point& top_left,
                           bool src_rect_specified,
                           const PP_Rect& src_rect);
  void OnMsgScroll(const HostResource& graphics_2d,
                   bool clip_specified,
                   const PP_Rect& clip,
                   const PP_Point& amount);
  void OnMsgReplaceContents(const HostResource& graphics_2d,
                            const HostResource& image_data);
  void OnMsgFlush(const HostResource& graphics_2d);

  // Renderer->plugin message handlers.
  void OnMsgFlushACK(const HostResource& graphics_2d,
                     int32_t pp_error);

  // Called in the renderer to send the given flush ACK to the plugin.
  void SendFlushACKToPlugin(int32_t result,
                            const HostResource& graphics_2d);

  CompletionCallbackFactory<PPB_Graphics2D_Proxy,
                            ProxyNonThreadSafeRefCount> callback_factory_;
};

class Graphics2D : public PluginResource,
                   public ::ppapi::thunk::PPB_Graphics2D_API {
 public:
  Graphics2D(const HostResource& host_resource,
             const PP_Size& size,
             PP_Bool is_always_opaque)
      : PluginResource(host_resource),
        size_(size),
        is_always_opaque_(is_always_opaque),
        current_flush_callback_(PP_BlockUntilComplete()) {
  }
  virtual ~Graphics2D() {
  }

  // Resource overrides.
  virtual Graphics2D* AsGraphics2D() { return this; }

  const PP_Size& size() const { return size_; }
  PP_Bool is_always_opaque() const { return is_always_opaque_; }

  bool is_flush_pending() const { return !!current_flush_callback_.func; }

  PP_CompletionCallback current_flush_callback() const {
    return current_flush_callback_;
  }
  void set_current_flush_callback(PP_CompletionCallback cb) {
    current_flush_callback_ = cb;
  }

  // ResourceObjectBase.
  virtual PPB_Graphics2D_API* AsGraphics2D_API();

  // PPB_Graphics_2D_API.
  PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque);
  void PaintImageData(PP_Resource image_data,
                      const PP_Point* top_left,
                      const PP_Rect* src_rect);
  void Scroll(const PP_Rect* clip_rect,
              const PP_Point* amount);
  void ReplaceContents(PP_Resource image_data);
  int32_t Flush(PP_CompletionCallback callback);

 private:
  PP_Size size_;
  PP_Bool is_always_opaque_;

  // In the plugin, this is the current callback set for Flushes. When the
  // callback function pointer is non-NULL, we're waiting for a flush ACK.
  PP_CompletionCallback current_flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(Graphics2D);
};


}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_GRAPHICS_2D_PROXY_H_
