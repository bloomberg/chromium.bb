// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BLIT_H_
#define UI_GFX_BLIT_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace skia {
class PlatformCanvas;
}  // namespace skia

namespace gfx {

class Point;
class Rect;

// Blits a rectangle from the source context into the destination context.
void BlitContextToContext(NativeDrawingContext dst_context,
                          const Rect& dst_rect,
                          NativeDrawingContext src_context,
                          const Point& src_origin);

// Blits a rectangle from the source context into the destination canvas.
void BlitContextToCanvas(skia::PlatformCanvas *dst_canvas,
                         const Rect& dst_rect,
                         NativeDrawingContext src_context,
                         const Point& src_origin);

// Blits a rectangle from the source canvas into the destination context.
void BlitCanvasToContext(NativeDrawingContext dst_context,
                         const Rect& dst_rect,
                         skia::PlatformCanvas *src_canvas,
                         const Point& src_origin);

// Blits a rectangle from the source canvas into the destination canvas.
void BlitCanvasToCanvas(skia::PlatformCanvas *dst_canvas,
                        const Rect& dst_rect,
                        skia::PlatformCanvas *src_canvas,
                        const Point& src_origin);

// Scrolls the given subset of the given canvas by the given amount.
// The canvas should not have a clip or a transform applied, since platforms
// may implement those operations differently.
void ScrollCanvas(skia::PlatformCanvas* canvas,
                  const Rect& clip,
                  const Point& amount);

}  // namespace gfx

#endif  // UI_GFX_BLIT_H_
