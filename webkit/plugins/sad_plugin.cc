// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/sad_plugin.h"

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/rect.h"

namespace webkit {

void PaintSadPlugin(WebKit::WebCanvas* webcanvas,
                    const gfx::Rect& plugin_rect,
                    const SkBitmap& sad_plugin_bitmap) {
  const int width = plugin_rect.width();
  const int height = plugin_rect.height();

#if WEBKIT_USING_SKIA
  SkCanvas* canvas = webcanvas;
#elif WEBKIT_USING_CG
  // Make a temporary canvas for the background image.
  scoped_ptr<SkCanvas> canvas(skia::CreateBitmapCanvas(width, height, false));
  // Flip the canvas, since the context expects flipped data.
  canvas->translate(0, height);
  canvas->scale(1, -1);
#endif

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRectCoords(0, 0, SkIntToScalar(width), SkIntToScalar(height),
                         paint);
  canvas->drawBitmap(
      sad_plugin_bitmap,
      SkIntToScalar(std::max(0, (width - sad_plugin_bitmap.width()) / 2)),
      SkIntToScalar(std::max(0, (height - sad_plugin_bitmap.height()) / 2)));

  // It's slightly less code to make a big SkBitmap of the sad tab image and
  // then copy that to the screen than to use the native APIs. The small speed
  // penalty is not important when drawing crashed plugins.
#if WEBKIT_USING_CG
  BlitCanvasToContext(webcanvas, plugin_rect, canvas.get(), gfx::Point(0, 0));
#endif
}

}  // namespace webkit
