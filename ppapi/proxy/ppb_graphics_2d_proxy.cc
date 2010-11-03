// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_graphics_2d_proxy.h"

#include <string.h>  // For memcpy

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
  Graphics2D(const PP_Size& size, bool is_always_opaque)
      : size_(size), is_always_opaque_(is_always_opaque) {
  }

  // Resource overrides.
  virtual Graphics2D* AsGraphics2D() { return this; }

  const PP_Size& size() const { return size_; }
  bool is_always_opaque() const { return is_always_opaque_; }

 private:
  PP_Size size_;
  bool is_always_opaque_;

  DISALLOW_COPY_AND_ASSIGN(Graphics2D);
};

namespace {

PP_Resource Create(PP_Module module_id,
                   const PP_Size* size,
                   bool is_always_opaque) {
  PluginDispatcher* dispatcher = PluginDispatcher::Get();
  PP_Resource result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_Create(
      INTERFACE_ID_PPB_GRAPHICS_2D, module_id, *size, is_always_opaque,
      &result));
  if (result) {
    linked_ptr<Graphics2D> graphics_2d(new Graphics2D(*size, is_always_opaque));
    dispatcher->plugin_resource_tracker()->AddResource(result, graphics_2d);
  }
  return result;
}

bool IsGraphics2D(PP_Resource resource) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(resource);
  return !!object;
}

bool Describe(PP_Resource graphics_2d,
              PP_Size* size,
              bool* is_always_opaque) {
  Graphics2D* object = PluginResource::GetAs<Graphics2D>(graphics_2d);
  if (!object) {
    size->width = 0;
    size->height = 0;
    *is_always_opaque = false;
    return false;
  }

  *size = object->size();
  *is_always_opaque = object->is_always_opaque();
  return true;
}

void PaintImageData(PP_Resource graphics_2d,
                    PP_Resource image_data,
                    const PP_Point* top_left,
                    const PP_Rect* src_rect) {
  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBGraphics2D_PaintImageData(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, image_data, *top_left,
      !!src_rect, src_rect ? *src_rect : dummy));
}

void Scroll(PP_Resource graphics_2d,
            const PP_Rect* clip_rect,
            const PP_Point* amount) {
  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBGraphics2D_Scroll(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, !!clip_rect,
      clip_rect ? *clip_rect : dummy, *amount));
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBGraphics2D_ReplaceContents(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d, image_data));
}

int32_t Flush(PP_Resource graphics_2d,
              PP_CompletionCallback callback) {
  PluginDispatcher* dispatcher = PluginDispatcher::Get();
  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics2D_Flush(
      INTERFACE_ID_PPB_GRAPHICS_2D, graphics_2d,
      dispatcher->callback_tracker().SendCallback(callback),
      &result));
  return result;
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
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Graphics2D_Proxy::~PPB_Graphics2D_Proxy() {
}

const void* PPB_Graphics2D_Proxy::GetSourceInterface() const {
  return &ppb_graphics_2d;
}

InterfaceID PPB_Graphics2D_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_GRAPHICS_2D;
}

void PPB_Graphics2D_Proxy::OnMessageReceived(const IPC::Message& msg) {
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
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
}

void PPB_Graphics2D_Proxy::OnMsgCreate(PP_Module module,
                                       const PP_Size& size,
                                       bool is_always_opaque,
                                       PP_Resource* result) {
  *result = ppb_graphics_2d_target()->Create(
      module, &size, is_always_opaque);
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

void PPB_Graphics2D_Proxy::OnMsgFlush(PP_Resource graphics_2d,
                                      uint32_t serialized_callback,
                                      int32_t* result) {
  // TODO(brettw) this should be a non-sync function. Ideally it would call
  // the callback with a failure code from here if you weren't allowed to
  // call Flush there.
  *result = ppb_graphics_2d_target()->Flush(
      graphics_2d, ReceiveCallback(serialized_callback));
}

}  // namespace proxy
}  // namespace pp
