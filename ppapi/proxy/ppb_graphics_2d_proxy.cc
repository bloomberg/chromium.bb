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
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

class Graphics2D : public PluginResource {
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

 private:
  PP_Size size_;
  PP_Bool is_always_opaque_;

  // In the plugin, this is the current callback set for Flushes. When the
  // callback function pointer is non-NULL, we're waiting for a flush ACK.
  PP_CompletionCallback current_flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(Graphics2D);
};

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_Size* size,
                   PP_Bool is_always_opaque) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_Create(
      INTERFACE_ID_PPB_GRAPHICS_2D, instance, *size, is_always_opaque,
      &result));
  if (result.is_null())
    return 0;
  linked_ptr<Graphics2D> graphics_2d(new Graphics2D(result, *size,
                                                    is_always_opaque));
  return PluginResourceTracker::GetInstance()->AddResource(graphics_2d);
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool Describe(PP_Resource graphics_2d,
                 PP_Size* size,
                 PP_Bool* is_always_opaque) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!object) {
    size->width = 0;
    size->height = 0;
    *is_always_opaque = PP_FALSE;
    return PP_FALSE;
  }

  *size = object->size();
  *is_always_opaque = object->is_always_opaque();
  return PP_TRUE;
}

void PaintImageData(PP_Resource graphics_2d,
                    PP_Resource image_data,
                    const PP_Point* top_left,
                    const PP_Rect* src_rect) {
  Graphics2D* graphics_object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!graphics_object)
    return;
  PluginResource* image_object = PluginResourceTracker::GetInstance()->
      GetResourceObject(image_data);
  if (!image_object)
    return;
  if (graphics_object->instance() != image_object->instance())
    return;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      graphics_object->instance());
  if (!dispatcher)
    return;

  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_PaintImageData(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_object->host_resource(),
      image_object->host_resource(), *top_left, !!src_rect,
      src_rect ? *src_rect : dummy));
}

void Scroll(PP_Resource graphics_2d,
            const PP_Rect* clip_rect,
            const PP_Point* amount) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!object)
    return;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return;

  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_Scroll(
      INTERFACE_ID_PPB_GRAPHICS_2D, object->host_resource(),
      !!clip_rect, clip_rect ? *clip_rect : dummy, *amount));
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  Graphics2D* graphics_object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!graphics_object)
    return;
  PluginResource* image_object = PluginResourceTracker::GetInstance()->
      GetResourceObject(image_data);
  if (!image_object)
    return;
  if (graphics_object->instance() != image_object->instance())
    return;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      graphics_object->instance());
  if (!dispatcher)
    return;

  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_ReplaceContents(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_object->host_resource(),
      image_object->host_resource()));
}

int32_t Flush(PP_Resource graphics_2d,
              PP_CompletionCallback callback) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!object)
    return PP_ERROR_BADRESOURCE;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return PP_ERROR_FAILED;

  // For now, disallow blocking calls. We'll need to add support for other
  // threads to this later.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (object->is_flush_pending())
    return PP_ERROR_INPROGRESS;  // Can't have >1 flush pending.
  object->set_current_flush_callback(callback);

  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_Flush(
      INTERFACE_ID_PPB_GRAPHICS_2D, object->host_resource()));
  return PP_ERROR_WOULDBLOCK;
}

const PPB_Graphics2D graphics_2d_interface = {
  &Create,
  &IsGraphics2D,
  &Describe,
  &PaintImageData,
  &Scroll,
  &ReplaceContents,
  &Flush
};

InterfaceProxy* CreateGraphics2DProxy(Dispatcher* dispatcher,
                                      const void* target_interface) {
  return new PPB_Graphics2D_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Graphics2D_Proxy::PPB_Graphics2D_Proxy(Dispatcher* dispatcher,
                                           const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics2D_Proxy::~PPB_Graphics2D_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Graphics2D_Proxy::GetInfo() {
  static const Info info = {
    &graphics_2d_interface,
    PPB_GRAPHICS_2D_INTERFACE,
    INTERFACE_ID_PPB_GRAPHICS_2D,
    false,
    &CreateGraphics2DProxy,
  };
  return &info;
}

bool PPB_Graphics2D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Graphics2D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics2D_Create,
                        OnMsgCreate)
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

void PPB_Graphics2D_Proxy::OnMsgCreate(PP_Instance instance,
                                       const PP_Size& size,
                                       PP_Bool is_always_opaque,
                                       HostResource* result) {
  result->SetHostResource(instance, ppb_graphics_2d_target()->Create(
      instance, &size, is_always_opaque));
}

void PPB_Graphics2D_Proxy::OnMsgPaintImageData(
    const HostResource& graphics_2d,
    const HostResource& image_data,
    const PP_Point& top_left,
    bool src_rect_specified,
    const PP_Rect& src_rect) {
  ppb_graphics_2d_target()->PaintImageData(
      graphics_2d.host_resource(), image_data.host_resource(), &top_left,
      src_rect_specified ? &src_rect : NULL);
}

void PPB_Graphics2D_Proxy::OnMsgScroll(const HostResource& graphics_2d,
                                       bool clip_specified,
                                       const PP_Rect& clip,
                                       const PP_Point& amount) {
  ppb_graphics_2d_target()->Scroll(graphics_2d.host_resource(),
                                   clip_specified ? &clip : NULL, &amount);
}

void PPB_Graphics2D_Proxy::OnMsgReplaceContents(
    const HostResource& graphics_2d,
    const HostResource& image_data) {
  ppb_graphics_2d_target()->ReplaceContents(graphics_2d.host_resource(),
                                            image_data.host_resource());
}

void PPB_Graphics2D_Proxy::OnMsgFlush(const HostResource& graphics_2d) {
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Graphics2D_Proxy::SendFlushACKToPlugin, graphics_2d);
  int32_t result = ppb_graphics_2d_target()->Flush(
      graphics_2d.host_resource(), callback.pp_completion_callback());
  if (result != PP_ERROR_WOULDBLOCK) {
    // There was some error, so we won't get a flush callback. We need to now
    // issue the ACK to the plugin hears about the error. This will also clean
    // up the data associated with the callback.
    callback.Run(result);
  }
}

void PPB_Graphics2D_Proxy::OnMsgFlushACK(const HostResource& host_resource,
                                         int32_t pp_error) {
  PP_Resource plugin_resource =
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
          host_resource);
  if (!plugin_resource)
    return;

  Graphics2D* object = PluginResource::GetAs<Graphics2D>(plugin_resource);
  if (!object) {
    // The plugin has released the graphics 2D object so don't issue the
    // callback.
    return;
  }

  // Be careful to make the callback NULL again before issuing the callback
  // since the plugin might want to flush from within the callback.
  PP_CompletionCallback callback = object->current_flush_callback();
  object->set_current_flush_callback(PP_BlockUntilComplete());
  PP_RunCompletionCallback(&callback, pp_error);
}

void PPB_Graphics2D_Proxy::SendFlushACKToPlugin(
    int32_t result,
    const HostResource& graphics_2d) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics2D_FlushACK(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, result));
}

}  // namespace proxy
}  // namespace pp
