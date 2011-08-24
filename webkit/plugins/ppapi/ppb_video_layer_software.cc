// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_layer_software.h"

namespace webkit {
namespace ppapi {

PPB_VideoLayer_Software::PPB_VideoLayer_Software(PP_Instance instance)
    : PPB_VideoLayer_Impl(instance) {
}

PPB_VideoLayer_Software::~PPB_VideoLayer_Software() {
}

void PPB_VideoLayer_Software::SetPixelFormat(
    PP_VideoLayerPixelFormat_Dev pixel_format) {
}

void PPB_VideoLayer_Software::SetNativeSize(const PP_Size* size) {
}

void PPB_VideoLayer_Software::SetClipRect(const PP_Rect* clip_rect) {
}

PP_Bool PPB_VideoLayer_Software::IsReady() {
  return PP_FALSE;
}

PP_Bool PPB_VideoLayer_Software::UpdateContent(uint32_t no_of_planes,
                                               const void** planes) {
  return PP_FALSE;
}

}  // namespace ppapi
}  // namespace webkit
