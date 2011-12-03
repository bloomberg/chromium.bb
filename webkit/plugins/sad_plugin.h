// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_SAD_PLUGIN_H_
#define WEBKIT_PLUGINS_SAD_PLUGIN_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCanvas.h"
#include "webkit/plugins/webkit_plugins_export.h"

class SkBitmap;

namespace gfx {
class Rect;
}

namespace webkit {

// Paints the sad plugin to the given canvas for the given plugin bounds. This
// is used by both the NPAPI and the PPAPI out-of-process plugin impls.
WEBKIT_PLUGINS_EXPORT void PaintSadPlugin(WebKit::WebCanvas* canvas,
                                          const gfx::Rect& plugin_rect,
                                          const SkBitmap& sad_plugin_bitmap);

}  // namespace

#endif  // WEBKIT_PLUGINS_SAD_PLUGIN_H_
