// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/webkit_forwarding.h"

namespace ppapi {

WebKitForwarding::Font::DrawTextParams::DrawTextParams(
    skia::PlatformCanvas* destination_arg,
    const TextRun& text_arg,
    const PP_Point* position_arg,
    uint32_t color_arg,
    const PP_Rect* clip_arg,
    PP_Bool image_data_is_opaque_arg)
    : destination(destination_arg),
      text(text_arg),
      position(position_arg),
      color(color_arg),
      clip(clip_arg),
      image_data_is_opaque(image_data_is_opaque_arg) {
}

WebKitForwarding::Font::DrawTextParams::~DrawTextParams() {
}

WebKitForwarding::Font::~Font() {
}

WebKitForwarding::~WebKitForwarding() {
}

}  // namespace ppapi

