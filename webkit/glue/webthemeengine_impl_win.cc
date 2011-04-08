// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_win.h"

#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "ui/gfx/native_theme_win.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;

namespace webkit_glue {

static RECT WebRectToRECT(const WebRect& rect) {
  RECT result;
  result.left = rect.x;
  result.top = rect.y;
  result.right = rect.x + rect.width;
  result.bottom = rect.y + rect.height;
  return result;
}

void WebThemeEngineImpl::paintButton(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintButton(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintMenuList(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintMenuList(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintScrollbarArrow(
    WebCanvas* canvas, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintScrollbarArrow(
      hdc, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintScrollbarThumb(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintScrollbarThumb(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintScrollbarTrack(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, const WebRect& align_rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  RECT native_align_rect = WebRectToRECT(align_rect);
  gfx::NativeThemeWin::instance()->PaintScrollbarTrack(
      hdc, part, state, classic_state, &native_rect, &native_align_rect,
      canvas);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintSpinButton(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintSpinButton(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintTextField(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, WebColor color, bool fill_content_area,
    bool draw_edges) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  COLORREF c = skia::SkColorToCOLORREF(color);

  gfx::NativeThemeWin::instance()->PaintTextField(
      hdc, part, state, classic_state, &native_rect, c, fill_content_area,
      draw_edges);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintTrackbar(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintTrackbar(
      hdc, part, state, classic_state, &native_rect, canvas);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintProgressBar(
    WebCanvas* canvas, const WebRect& barRect, const WebRect& valueRect,
    bool determinate, double animatedSeconds)
{
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_bar_rect = WebRectToRECT(barRect);
  RECT native_value_rect = WebRectToRECT(valueRect);
  gfx::NativeThemeWin::instance()->PaintProgressBar(
      hdc, &native_bar_rect,
      &native_value_rect, determinate, animatedSeconds, canvas);

  skia::EndPlatformPaint(canvas);
}

}  // namespace webkit_glue
