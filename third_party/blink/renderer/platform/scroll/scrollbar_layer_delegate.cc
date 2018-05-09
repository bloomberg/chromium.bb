// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scroll/scrollbar_layer_delegate.h"

#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_scrollbar.h"
#include "third_party/blink/public/platform/web_scrollbar_theme_geometry.h"

namespace blink {

ScrollbarLayerDelegate::ScrollbarLayerDelegate(
    std::unique_ptr<WebScrollbar> scrollbar,
    WebScrollbarThemePainter painter,
    std::unique_ptr<WebScrollbarThemeGeometry> geometry)
    : scrollbar_(std::move(scrollbar)),
      painter_(painter),
      geometry_(std::move(geometry)) {}

ScrollbarLayerDelegate::~ScrollbarLayerDelegate() = default;

cc::ScrollbarOrientation ScrollbarLayerDelegate::Orientation() const {
  if (scrollbar_->GetOrientation() == WebScrollbar::kHorizontal)
    return cc::HORIZONTAL;
  return cc::VERTICAL;
}

bool ScrollbarLayerDelegate::IsLeftSideVerticalScrollbar() const {
  return scrollbar_->IsLeftSideVerticalScrollbar();
}

bool ScrollbarLayerDelegate::HasThumb() const {
  return geometry_->HasThumb(scrollbar_.get());
}

bool ScrollbarLayerDelegate::IsOverlay() const {
  return scrollbar_->IsOverlay();
}

gfx::Point ScrollbarLayerDelegate::Location() const {
  return static_cast<gfx::Point>(scrollbar_->Location());
}

int ScrollbarLayerDelegate::ThumbThickness() const {
  auto thumb_rect =
      static_cast<gfx::Rect>(geometry_->ThumbRect(scrollbar_.get()));
  if (scrollbar_->GetOrientation() == WebScrollbar::kHorizontal)
    return thumb_rect.height();
  return thumb_rect.width();
}

int ScrollbarLayerDelegate::ThumbLength() const {
  auto thumb_rect =
      static_cast<gfx::Rect>(geometry_->ThumbRect(scrollbar_.get()));
  if (scrollbar_->GetOrientation() == WebScrollbar::kHorizontal)
    return thumb_rect.width();
  return thumb_rect.height();
}

gfx::Rect ScrollbarLayerDelegate::TrackRect() const {
  return static_cast<gfx::Rect>(geometry_->TrackRect(scrollbar_.get()));
}

float ScrollbarLayerDelegate::ThumbOpacity() const {
  return painter_.ThumbOpacity();
}

bool ScrollbarLayerDelegate::NeedsPaintPart(cc::ScrollbarPart part) const {
  if (part == cc::THUMB)
    return painter_.ThumbNeedsRepaint();
  return painter_.TrackNeedsRepaint();
}

bool ScrollbarLayerDelegate::UsesNinePatchThumbResource() const {
  return painter_.UsesNinePatchThumbResource();
}

gfx::Size ScrollbarLayerDelegate::NinePatchThumbCanvasSize() const {
  return static_cast<gfx::Size>(
      geometry_->NinePatchThumbCanvasSize(scrollbar_.get()));
}

gfx::Rect ScrollbarLayerDelegate::NinePatchThumbAperture() const {
  return static_cast<gfx::Rect>(
      geometry_->NinePatchThumbAperture(scrollbar_.get()));
}

bool ScrollbarLayerDelegate::HasTickmarks() const {
  return scrollbar_->HasTickmarks();
}

void ScrollbarLayerDelegate::PaintPart(cc::PaintCanvas* canvas,
                                       cc::ScrollbarPart part,
                                       const gfx::Rect& content_rect) {
  if (part == cc::THUMB) {
    painter_.PaintThumb(canvas, WebRect(content_rect));
    return;
  }

  if (part == cc::TICKMARKS) {
    painter_.PaintTickmarks(canvas, WebRect(content_rect));
    return;
  }

  // The following is a simplification of ScrollbarThemeComposite::paint.
  painter_.PaintScrollbarBackground(canvas, WebRect(content_rect));

  if (geometry_->HasButtons(scrollbar_.get())) {
    WebRect back_button_start_paint_rect =
        geometry_->BackButtonStartRect(scrollbar_.get());
    painter_.PaintBackButtonStart(canvas, back_button_start_paint_rect);

    WebRect back_button_end_paint_rect =
        geometry_->BackButtonEndRect(scrollbar_.get());
    painter_.PaintBackButtonEnd(canvas, back_button_end_paint_rect);

    WebRect forward_button_start_paint_rect =
        geometry_->ForwardButtonStartRect(scrollbar_.get());
    painter_.PaintForwardButtonStart(canvas, forward_button_start_paint_rect);

    WebRect forward_button_end_paint_rect =
        geometry_->ForwardButtonEndRect(scrollbar_.get());
    painter_.PaintForwardButtonEnd(canvas, forward_button_end_paint_rect);
  }

  WebRect track_paint_rect = geometry_->TrackRect(scrollbar_.get());
  painter_.PaintTrackBackground(canvas, track_paint_rect);

  bool thumb_present = geometry_->HasThumb(scrollbar_.get());
  if (thumb_present) {
    painter_.PaintForwardTrackPart(canvas, track_paint_rect);
    painter_.PaintBackTrackPart(canvas, track_paint_rect);
  }

  painter_.PaintTickmarks(canvas, track_paint_rect);
}

}  // namespace blink
