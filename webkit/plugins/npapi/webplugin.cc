// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugin.h"

namespace webkit {
namespace npapi {

WebPluginGeometry::WebPluginGeometry()
    : window(gfx::kNullPluginWindow),
      rects_valid(false),
      visible(false) {
}

WebPluginGeometry::~WebPluginGeometry() {
}

bool WebPluginGeometry::Equals(const WebPluginGeometry& rhs) const {
  return window == rhs.window &&
         window_rect == rhs.window_rect &&
         clip_rect == rhs.clip_rect &&
         cutout_rects == rhs.cutout_rects &&
         rects_valid == rhs.rects_valid &&
         visible == rhs.visible;
}

#if defined(OS_MACOSX)
WebPluginAcceleratedSurface* WebPlugin::GetAcceleratedSurface() {
  return NULL;
}
#endif

WebPluginDelegate* WebPlugin::delegate() {
  return NULL;
}

}  // namespace npapi
}  // namespace webkit
