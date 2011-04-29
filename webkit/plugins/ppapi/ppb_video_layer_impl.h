// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_IMPL_H_

#include "ppapi/c/dev/ppb_video_layer_dev.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_Rect;
struct PP_Size;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_VideoLayer_Impl : public Resource {
 public:
  explicit PPB_VideoLayer_Impl(PluginInstance* instance);
  virtual ~PPB_VideoLayer_Impl();

  static const PPB_VideoLayer_Dev* GetInterface();

  // Resource override.
  virtual PPB_VideoLayer_Impl* AsPPB_VideoLayer_Impl();

  // Pure virtual methods to be implemented by subclasses.
  virtual void SetPixelFormat(PP_VideoLayerPixelFormat_Dev pixel_format) = 0;
  virtual void SetNativeSize(const PP_Size* size) = 0;
  virtual void SetClipRect(const PP_Rect* clip_rect) = 0;
  virtual PP_Bool IsReady() = 0;
  virtual PP_Bool UpdateContent(uint32_t no_of_planes, const void** planes) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_VideoLayer_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_IMPL_H_
