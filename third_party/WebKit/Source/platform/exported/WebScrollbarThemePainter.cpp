/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebScrollbarThemePainter.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/WebRect.h"

namespace blink {

// WebScrollbarThemeMac uses GraphicsContextCanvas which doesn't quite support
// device clip rects not at the origin.  This class translates the recording
// canvas to the origin and then adjusts it back during playback.
class ScopedScrollbarPainter {
 public:
  ScopedScrollbarPainter(WebScrollbarThemePainter* painter,
                         WebCanvas* canvas,
                         const WebRect& rect)
      : int_rect_(IntRect(IntPoint(), IntSize(rect.width, rect.height))),
        canvas_(canvas),
        rect_(rect) {
    builder_.Context().SetDeviceScaleFactor(painter->DeviceScaleFactor());
  }
  GraphicsContext& Context() { return builder_.Context(); }
  const IntRect& Rect() const { return int_rect_; }

  ~ScopedScrollbarPainter() {
    canvas_->save();
    canvas_->translate(rect_.x, rect_.y);
    canvas_->drawPicture(builder_.EndRecording());
    canvas_->restore();
  }

 protected:
  IntRect int_rect_;
  PaintRecordBuilder builder_;
  WebCanvas* canvas_;
  const WebRect& rect_;
};

void WebScrollbarThemePainter::Assign(const WebScrollbarThemePainter& painter) {
  // This is a pointer to a static object, so no ownership transferral.
  theme_ = painter.theme_;
  scrollbar_ = painter.scrollbar_;
  device_scale_factor_ = painter.device_scale_factor_;
}

void WebScrollbarThemePainter::Reset() {
  scrollbar_ = nullptr;
}

void WebScrollbarThemePainter::PaintScrollbarBackground(WebCanvas* canvas,
                                                        const WebRect& rect) {
  SkRect clip = SkRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
  canvas->clipRect(clip);

  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintScrollbarBackground(painter.Context(), *scrollbar_);
}

void WebScrollbarThemePainter::PaintTrackBackground(WebCanvas* canvas,
                                                    const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintTrackBackground(painter.Context(), *scrollbar_, painter.Rect());
  if (!theme_->ShouldRepaintAllPartsOnInvalidation())
    scrollbar_->ClearTrackNeedsRepaint();
}

void WebScrollbarThemePainter::PaintBackTrackPart(WebCanvas* canvas,
                                                  const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintTrackPiece(painter.Context(), *scrollbar_, painter.Rect(),
                          kBackTrackPart);
}

void WebScrollbarThemePainter::PaintForwardTrackPart(WebCanvas* canvas,
                                                     const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintTrackPiece(painter.Context(), *scrollbar_, painter.Rect(),
                          kForwardTrackPart);
}

void WebScrollbarThemePainter::PaintBackButtonStart(WebCanvas* canvas,
                                                    const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintButton(painter.Context(), *scrollbar_, painter.Rect(),
                      kBackButtonStartPart);
}

void WebScrollbarThemePainter::PaintBackButtonEnd(WebCanvas* canvas,
                                                  const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintButton(painter.Context(), *scrollbar_, painter.Rect(),
                      kBackButtonEndPart);
}

void WebScrollbarThemePainter::PaintForwardButtonStart(WebCanvas* canvas,
                                                       const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintButton(painter.Context(), *scrollbar_, painter.Rect(),
                      kForwardButtonStartPart);
}

void WebScrollbarThemePainter::PaintForwardButtonEnd(WebCanvas* canvas,
                                                     const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintButton(painter.Context(), *scrollbar_, painter.Rect(),
                      kForwardButtonEndPart);
}

void WebScrollbarThemePainter::PaintTickmarks(WebCanvas* canvas,
                                              const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintTickmarks(painter.Context(), *scrollbar_, painter.Rect());
}

void WebScrollbarThemePainter::PaintThumb(WebCanvas* canvas,
                                          const WebRect& rect) {
  ScopedScrollbarPainter painter(this, canvas, rect);
  theme_->PaintThumb(painter.Context(), *scrollbar_, painter.Rect());
  if (!theme_->ShouldRepaintAllPartsOnInvalidation())
    scrollbar_->ClearThumbNeedsRepaint();
}

WebScrollbarThemePainter::WebScrollbarThemePainter(ScrollbarTheme& theme,
                                                   Scrollbar& scrollbar,
                                                   float device_scale_factor)
    : theme_(&theme),
      scrollbar_(&scrollbar),
      device_scale_factor_(device_scale_factor) {}

float WebScrollbarThemePainter::ThumbOpacity() const {
  return theme_->ThumbOpacity(*scrollbar_);
}

bool WebScrollbarThemePainter::TrackNeedsRepaint() const {
  return scrollbar_->TrackNeedsRepaint();
}

bool WebScrollbarThemePainter::ThumbNeedsRepaint() const {
  return scrollbar_->ThumbNeedsRepaint();
}

bool WebScrollbarThemePainter::UsesNinePatchThumbResource() const {
  return theme_->UsesNinePatchThumbResource();
}

}  // namespace blink
