// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_VIDEO_LAYER_API_H_
#define PPAPI_THUNK_VIDEO_LAYER_API_H_

#include "ppapi/c/dev/ppb_video_layer_dev.h"

namespace ppapi {
namespace thunk {

class PPB_VideoLayer_API {
 public:
  virtual ~PPB_VideoLayer_API() {}

  virtual void SetPixelFormat(PP_VideoLayerPixelFormat_Dev pixel_format) = 0;
  virtual void SetNativeSize(const PP_Size* size) = 0;
  virtual void SetClipRect(const PP_Rect* clip_rect) = 0;
  virtual PP_Bool IsReady() = 0;
  virtual PP_Bool UpdateContent(uint32_t no_of_planes, const void** planes) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_VIDEO_LAYER_API_H_
