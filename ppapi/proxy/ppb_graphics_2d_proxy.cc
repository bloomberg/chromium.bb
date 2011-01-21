// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  Graphics2D(PP_Instance instance,
             const PP_Size& size,
             PP_Bool is_always_opaque)
      : PluginResource(instance),
        size_(size),
        is_always_opaque_(is_always_opaque),
        current_flush_callback_(PP_BlockUntilComplete()) {
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

  PP_Resource result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_Create(
      INTERFACE_ID_PPB_GRAPHICS_2D, instance, *size, is_always_opaque,
      &result));
  if (result) {
    linked_ptr<Graphics2D> graphics_2d(new Graphics2D(instance, *size,
                                                      is_always_opaque));
    PluginResourceTracker::GetInstance()->AddResource(result, graphics_2d);
  }
  return result;
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
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!object)
    return;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return;

  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_PaintImageData(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, image_data, *top_left,
      !!src_rect, src_rect ? *src_rect : dummy));
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
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, !!clip_rect,
      clip_rect ? *clip_rect : dummy, *amount));
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!object)
    return;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return;

  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_ReplaceContents(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, image_data));
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
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d));
  return PP_ERROR_WOULDBLOCK;
}

const PPB_Graphics2D ppb_graphics_2d = {
  &Create,
  &IsGraphics2D,
  &Describe,
  &PaintImageData,
  &Scroll,
  &ReplaceContents,
  &Flush
};

}  // namespace

PPB_Graphics2D_Proxy::PPB_Graphics2D_Proxy(Dispatcher* dispatcher,
                                           const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics2D_Proxy::~PPB_Graphics2D_Proxy() {
}

const void* PPB_Graphics2D_Proxy::GetSourceInterface() const {
  return &ppb_graphics_2d;
}

InterfaceID PPB_Graphics2D_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_GRAPHICS_2D;
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
                                       PP_Resource* result) {
  *result = ppb_graphics_2d_target()->Create(
      instance, &size, is_always_opaque);
}

void PPB_Graphics2D_Proxy::OnMsgPaintImageData(PP_Resource graphics_2d,
                                               PP_Resource image_data,
                                               const PP_Point& top_left,
                                               bool src_rect_specified,
                                               const PP_Rect& src_rect) {
  ppb_graphics_2d_target()->PaintImageData(
      graphics_2d, image_data, &top_left,
      src_rect_specified ? &src_rect : NULL);
}

void PPB_Graphics2D_Proxy::OnMsgScroll(PP_Resource graphics_2d,
                                       bool clip_specified,
                                       const PP_Rect& clip,
                                       const PP_Point& amount) {
  ppb_graphics_2d_target()->Scroll(
      graphics_2d,
      clip_specified ? &clip : NULL, &amount);
}

void PPB_Graphics2D_Proxy::OnMsgReplaceContents(PP_Resource graphics_2d,
                                                PP_Resource image_data) {
  ppb_graphics_2d_target()->ReplaceContents(graphics_2d, image_data);
}

void PPB_Graphics2D_Proxy::OnMsgFlush(PP_Resource graphics_2d) {
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Graphics2D_Proxy::SendFlushACKToPlugin, graphics_2d);
  int32_t result = ppb_graphics_2d_target()->Flush(
      graphics_2d, callback.pp_completion_callback());
  if (result != PP_ERROR_WOULDBLOCK) {
    // There was some error, so we won't get a flush callback. We need to now
    // issue the ACK to the plugin hears about the error. This will also clean
    // up the data associated with the callback.
    callback.Run(result);
  }
}

void PPB_Graphics2D_Proxy::OnMsgFlushACK(PP_Resource resource,
                                         int32_t pp_error) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(resource);
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

void PPB_Graphics2D_Proxy::SendFlushACKToPlugin(int32_t result,
                                                PP_Resource graphics_2d) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics2D_FlushACK(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, result));
}

}  // namespace proxy
}  // namespace pp
