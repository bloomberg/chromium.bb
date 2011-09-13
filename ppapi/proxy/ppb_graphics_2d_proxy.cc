// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_graphics_2d_proxy.h"

#include <string.h>  // For memset.

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_Graphics2D_API;

namespace ppapi {
namespace proxy {

class Graphics2D : public Resource, public thunk::PPB_Graphics2D_API {
 public:
  Graphics2D(const HostResource& host_resource,
             const PP_Size& size,
             PP_Bool is_always_opaque);
  virtual ~Graphics2D();

  // Resource.
  virtual PPB_Graphics2D_API* AsPPB_Graphics2D_API();

  // PPB_Graphics_2D_API.
  PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque);
  void PaintImageData(PP_Resource image_data,
                      const PP_Point* top_left,
                      const PP_Rect* src_rect);
  void Scroll(const PP_Rect* clip_rect,
              const PP_Point* amount);
  void ReplaceContents(PP_Resource image_data);
  int32_t Flush(PP_CompletionCallback callback);

  // Notification that the host has sent an ACK for a pending Flush.
  void FlushACK(int32_t result_code);

 private:
  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  PP_Size size_;
  PP_Bool is_always_opaque_;

  // In the plugin, this is the current callback set for Flushes. When the
  // callback function pointer is non-NULL, we're waiting for a flush ACK.
  PP_CompletionCallback current_flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(Graphics2D);
};

Graphics2D::Graphics2D(const HostResource& host_resource,
                       const PP_Size& size,
                       PP_Bool is_always_opaque)
    : Resource(host_resource),
      size_(size),
      is_always_opaque_(is_always_opaque),
      current_flush_callback_(PP_BlockUntilComplete()) {
}

Graphics2D::~Graphics2D() {
}

PPB_Graphics2D_API* Graphics2D::AsPPB_Graphics2D_API() {
  return this;
}

PP_Bool Graphics2D::Describe(PP_Size* size, PP_Bool* is_always_opaque) {
  *size = size_;
  *is_always_opaque = is_always_opaque_;
  return PP_TRUE;
}

void Graphics2D::PaintImageData(PP_Resource image_data,
                                const PP_Point* top_left,
                                const PP_Rect* src_rect) {
  Resource* image_object = PluginResourceTracker::GetInstance()->
      GetResource(image_data);
  if (!image_object || pp_instance() != image_object->pp_instance())
    return;

  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  GetDispatcher()->Send(new PpapiHostMsg_PPBGraphics2D_PaintImageData(
      INTERFACE_ID_PPB_GRAPHICS_2D, host_resource(),
      image_object->host_resource(), *top_left, !!src_rect,
      src_rect ? *src_rect : dummy));
}

void Graphics2D::Scroll(const PP_Rect* clip_rect,
                        const PP_Point* amount) {
  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  GetDispatcher()->Send(new PpapiHostMsg_PPBGraphics2D_Scroll(
      INTERFACE_ID_PPB_GRAPHICS_2D, host_resource(),
      !!clip_rect, clip_rect ? *clip_rect : dummy, *amount));
}

void Graphics2D::ReplaceContents(PP_Resource image_data) {
  Resource* image_object = PluginResourceTracker::GetInstance()->
      GetResource(image_data);
  if (!image_object || pp_instance() != image_object->pp_instance())
    return;

  GetDispatcher()->Send(new PpapiHostMsg_PPBGraphics2D_ReplaceContents(
      INTERFACE_ID_PPB_GRAPHICS_2D, host_resource(),
      image_object->host_resource()));
}

int32_t Graphics2D::Flush(PP_CompletionCallback callback) {
  // For now, disallow blocking calls. We'll need to add support for other
  // threads to this later.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (current_flush_callback_.func)
    return PP_ERROR_INPROGRESS;  // Can't have >1 flush pending.
  current_flush_callback_ = callback;

  GetDispatcher()->Send(new PpapiHostMsg_PPBGraphics2D_Flush(
      INTERFACE_ID_PPB_GRAPHICS_2D, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

void Graphics2D::FlushACK(int32_t result_code) {
  PP_RunAndClearCompletionCallback(&current_flush_callback_, result_code);
}

PPB_Graphics2D_Proxy::PPB_Graphics2D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics2D_Proxy::~PPB_Graphics2D_Proxy() {
}

// static
PP_Resource PPB_Graphics2D_Proxy::CreateProxyResource(
    PP_Instance instance,
    const PP_Size& size,
    PP_Bool is_always_opaque) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_ResourceCreation_Graphics2D(
      INTERFACE_ID_RESOURCE_CREATION, instance, size, is_always_opaque,
      &result));
  if (result.is_null())
    return 0;
  return (new Graphics2D(result, size, is_always_opaque))->GetReference();
}

bool PPB_Graphics2D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Graphics2D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics2D_PaintImageData,
                        OnMsgPaintImageData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics2D_Scroll,
                        OnMsgScroll)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics2D_ReplaceContents,
                        OnMsgReplaceContents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics2D_Flush,
                        OnMsgFlush)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBGraphics2D_FlushACK,
                        OnMsgFlushACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

void PPB_Graphics2D_Proxy::OnMsgPaintImageData(
    const HostResource& graphics_2d,
    const HostResource& image_data,
    const PP_Point& top_left,
    bool src_rect_specified,
    const PP_Rect& src_rect) {
  EnterHostFromHostResource<PPB_Graphics2D_API> enter(graphics_2d);
  if (enter.failed())
    return;
  enter.object()->PaintImageData(image_data.host_resource(), &top_left,
      src_rect_specified ? &src_rect : NULL);
}

void PPB_Graphics2D_Proxy::OnMsgScroll(const HostResource& graphics_2d,
                                       bool clip_specified,
                                       const PP_Rect& clip,
                                       const PP_Point& amount) {
  EnterHostFromHostResource<PPB_Graphics2D_API> enter(graphics_2d);
  if (enter.failed())
    return;
  enter.object()->Scroll(clip_specified ? &clip : NULL, &amount);
}

void PPB_Graphics2D_Proxy::OnMsgReplaceContents(
    const HostResource& graphics_2d,
    const HostResource& image_data) {
  EnterHostFromHostResource<PPB_Graphics2D_API> enter(graphics_2d);
  if (enter.failed())
    return;
  enter.object()->ReplaceContents(image_data.host_resource());
}

void PPB_Graphics2D_Proxy::OnMsgFlush(const HostResource& graphics_2d) {
  EnterHostFromHostResourceForceCallback<PPB_Graphics2D_API> enter(
      graphics_2d, callback_factory_,
      &PPB_Graphics2D_Proxy::SendFlushACKToPlugin, graphics_2d);
  if (enter.failed())
    return;
  enter.SetResult(enter.object()->Flush(enter.callback()));
}

void PPB_Graphics2D_Proxy::OnMsgFlushACK(const HostResource& host_resource,
                                         int32_t pp_error) {
  EnterPluginFromHostResource<PPB_Graphics2D_API> enter(host_resource);
  if (enter.succeeded())
    static_cast<Graphics2D*>(enter.object())->FlushACK(pp_error);
}

void PPB_Graphics2D_Proxy::SendFlushACKToPlugin(
    int32_t result,
    const HostResource& graphics_2d) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics2D_FlushACK(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, result));
}

}  // namespace proxy
}  // namespace ppapi
