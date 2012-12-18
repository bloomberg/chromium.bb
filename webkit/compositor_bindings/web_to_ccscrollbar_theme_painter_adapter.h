// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_TO_CCSCROLLBAR_THEME_PAINTER_ADAPTER_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_TO_CCSCROLLBAR_THEME_PAINTER_ADAPTER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/scrollbar_theme_painter.h"

namespace WebKit {

class WebScrollbarThemePainter;

class WebToCCScrollbarThemePainterAdapter : public cc::ScrollbarThemePainter {
 public:
  static scoped_ptr<WebToCCScrollbarThemePainterAdapter> Create(
      scoped_ptr<WebScrollbarThemePainter> webPainter) {
    return make_scoped_ptr(new WebToCCScrollbarThemePainterAdapter(
        webPainter.Pass()));
  }
  virtual ~WebToCCScrollbarThemePainterAdapter();

  virtual void PaintScrollbarBackground(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintTrackBackground(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintBackTrackPart(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintForwardTrackPart(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintBackButtonStart(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintBackButtonEnd(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintForwardButtonStart(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintForwardButtonEnd(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintTickmarks(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;
  virtual void PaintThumb(SkCanvas* canvas, const gfx::Rect& rect)
      OVERRIDE;

 private:
  WebToCCScrollbarThemePainterAdapter(
      scoped_ptr<WebScrollbarThemePainter> webPainter)
      : painter_(webPainter.Pass()) {}

  scoped_ptr<WebScrollbarThemePainter> painter_;
};

}  // namespace WebKit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_TO_CCSCROLLBAR_THEME_PAINTER_ADAPTER_H_
