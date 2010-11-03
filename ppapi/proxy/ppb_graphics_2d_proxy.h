// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_GRAPHICS_2D_PROXY_H_
#define PPAPI_PPB_GRAPHICS_2D_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Graphics2D;
struct PP_Point;
struct PP_Rect;

namespace pp {
namespace proxy {

class PPB_Graphics2D_Proxy : public InterfaceProxy {
 public:
  PPB_Graphics2D_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Graphics2D_Proxy();

  const PPB_Graphics2D* ppb_graphics_2d_target() const {
    return reinterpret_cast<const PPB_Graphics2D*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Module module,
                   const PP_Size& size,
                   bool is_always_opaque,
                   PP_Resource* result);
  void OnMsgPaintImageData(PP_Resource graphics_2d,
                           PP_Resource image_data,
                           const PP_Point& top_left,
                           bool src_rect_specified,
                           const PP_Rect& src_rect);
  void OnMsgScroll(PP_Resource graphics_2d,
                   bool clip_specified,
                   const PP_Rect& clip,
                   const PP_Point& amount);
  void OnMsgReplaceContents(PP_Resource graphics_2d,
                            PP_Resource image_data);
  void OnMsgFlush(PP_Resource graphics_2d,
                  uint32_t serialized_callback,
                  int32_t* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_GRAPHICS_2D_PROXY_H_
