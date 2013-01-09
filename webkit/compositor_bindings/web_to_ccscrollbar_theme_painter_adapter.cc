// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_to_ccscrollbar_theme_painter_adapter.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemePainter.h"

namespace WebKit {

WebToCCScrollbarThemePainterAdapter::~WebToCCScrollbarThemePainterAdapter() {
}

void WebToCCScrollbarThemePainterAdapter::PaintScrollbarBackground(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintScrollbarBackground(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintTrackBackground(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintTrackBackground(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintBackTrackPart(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintBackTrackPart(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintForwardTrackPart(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintForwardTrackPart(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintBackButtonStart(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintBackButtonStart(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintBackButtonEnd(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintBackButtonEnd(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintForwardButtonStart(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintForwardButtonStart(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintForwardButtonEnd(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintForwardButtonEnd(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintTickmarks(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintTickmarks(canvas, rect);
}

void WebToCCScrollbarThemePainterAdapter::PaintThumb(
    SkCanvas* canvas, const gfx::Rect& rect) {
  painter_->paintThumb(canvas, rect);
}

}  // namespace WebKit
