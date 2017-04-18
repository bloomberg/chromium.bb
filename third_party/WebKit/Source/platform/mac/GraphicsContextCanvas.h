// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GraphicsContextCanvas_h
#define GraphicsContextCanvas_h

#include <ApplicationServices/ApplicationServices.h>

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "third_party/skia/include/core/SkBitmap.h"

struct SkIRect;

namespace blink {

// Converts a PaintCanvas temporarily to a CGContext
class PLATFORM_EXPORT GraphicsContextCanvas {
 public:
  /**
    User clip rect is an *additional* clip to be applied in addition to the
    current state of the canvas, in *local* rather than device coordinates.
    If no additional clipping is desired, pass in
    SkIRect::MakeSize(canvas->getBaseLayerSize()) transformed by the inverse
    CTM.
   */
  GraphicsContextCanvas(PaintCanvas*,
                        const SkIRect& user_clip_rect,
                        SkScalar bitmap_scale_factor = 1);
  ~GraphicsContextCanvas();
  CGContextRef CgContext();
  bool HasEmptyClipRegion() const;

 private:
  void ReleaseIfNeeded();
  SkIRect ComputeDirtyRect();

  PaintCanvas* canvas_;

  CGContextRef cg_context_;
  // m_offscreen is only valid if m_useDeviceBits is false
  SkBitmap offscreen_;
  SkIPoint bitmap_offset_;
  SkScalar bitmap_scale_factor_;

  // True if |bitmap_| is a dummy 1x1 bitmap allocated for the sake of creating
  // a non-null CGContext (it is invalid to use a null CGContext), and will not
  // be copied to |m_canvas|. This will happen if |m_canvas|'s clip region is
  // empty.
  bool bitmap_is_dummy_;
};

}  // namespace blink

#endif  // GraphicsContextCanvas_h
