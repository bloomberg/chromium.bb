// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_SOFTWARE_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_SOFTWARE_H_

#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"

namespace webkit {
namespace ppapi {

class PPB_VideoLayer_Software : public PPB_VideoLayer_Impl {
 public:
  explicit PPB_VideoLayer_Software(PP_Instance instance);
  virtual ~PPB_VideoLayer_Software();

  // PPB_VideoLayer_Impl implementations.
  virtual void SetPixelFormat(PP_VideoLayerPixelFormat_Dev pixel_format);
  virtual void SetNativeSize(const PP_Size* size);
  virtual void SetClipRect(const PP_Rect* clip_rect);
  virtual PP_Bool IsReady();
  virtual PP_Bool UpdateContent(uint32_t no_of_planes, const void** planes);

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_VideoLayer_Software);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_VIDEO_LAYER_SOFTWARE_H_
