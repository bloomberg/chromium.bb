// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"

#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "ui/gfx/skia_util.h"

namespace blink {

namespace {

class ScopedScrollbarPainter {
 public:
  ScopedScrollbarPainter(cc::PaintCanvas& canvas, float device_scale_factor)
      : canvas_(canvas) {
    builder_.Context().SetDeviceScaleFactor(device_scale_factor);
  }
  ~ScopedScrollbarPainter() { canvas_.drawPicture(builder_.EndRecording()); }

  GraphicsContext& Context() { return builder_.Context(); }

 private:
  cc::PaintCanvas& canvas_;
  PaintRecordBuilder builder_;
};

}  // namespace

ScrollbarLayerDelegate::ScrollbarLayerDelegate(blink::Scrollbar& scrollbar,
                                               float device_scale_factor)
    : scrollbar_(&scrollbar),
      theme_(scrollbar.GetTheme()),
      device_scale_factor_(device_scale_factor) {}

ScrollbarLayerDelegate::~ScrollbarLayerDelegate() = default;

cc::ScrollbarOrientation ScrollbarLayerDelegate::Orientation() const {
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return cc::HORIZONTAL;
  return cc::VERTICAL;
}

bool ScrollbarLayerDelegate::IsLeftSideVerticalScrollbar() const {
  return scrollbar_->IsLeftSideVerticalScrollbar();
}

bool ScrollbarLayerDelegate::HasThumb() const {
  return theme_.HasThumb(*scrollbar_);
}

bool ScrollbarLayerDelegate::IsOverlay() const {
  return scrollbar_->IsOverlayScrollbar();
}

gfx::Point ScrollbarLayerDelegate::Location() const {
  return scrollbar_->Location();
}

int ScrollbarLayerDelegate::ThumbThickness() const {
  IntRect thumb_rect = theme_.ThumbRect(*scrollbar_);
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return thumb_rect.Height();
  return thumb_rect.Width();
}

int ScrollbarLayerDelegate::ThumbLength() const {
  IntRect thumb_rect = theme_.ThumbRect(*scrollbar_);
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return thumb_rect.Width();
  return thumb_rect.Height();
}

gfx::Rect ScrollbarLayerDelegate::TrackRect() const {
  IntRect track_rect = theme_.TrackRect(*scrollbar_);
  track_rect.MoveBy(-scrollbar_->Location());
  return track_rect;
}

bool ScrollbarLayerDelegate::SupportsDragSnapBack() const {
  return theme_.SupportsDragSnapBack();
}

gfx::Rect ScrollbarLayerDelegate::BackButtonRect() const {
  IntRect back_button_rect =
      theme_.BackButtonRect(*scrollbar_, blink::kBackButtonStartPart);
  back_button_rect.MoveBy(-scrollbar_->Location());
  return back_button_rect;
}

gfx::Rect ScrollbarLayerDelegate::ForwardButtonRect() const {
  IntRect forward_button_rect =
      theme_.ForwardButtonRect(*scrollbar_, blink::kForwardButtonEndPart);
  forward_button_rect.MoveBy(-scrollbar_->Location());
  return forward_button_rect;
}

float ScrollbarLayerDelegate::ThumbOpacity() const {
  return theme_.ThumbOpacity(*scrollbar_);
}

bool ScrollbarLayerDelegate::NeedsPaintPart(cc::ScrollbarPart part) const {
  if (part == cc::THUMB)
    return scrollbar_->ThumbNeedsRepaint();
  return scrollbar_->TrackNeedsRepaint();
}

bool ScrollbarLayerDelegate::UsesNinePatchThumbResource() const {
  return theme_.UsesNinePatchThumbResource();
}

gfx::Size ScrollbarLayerDelegate::NinePatchThumbCanvasSize() const {
  DCHECK(theme_.UsesNinePatchThumbResource());
  return static_cast<gfx::Size>(theme_.NinePatchThumbCanvasSize(*scrollbar_));
}

gfx::Rect ScrollbarLayerDelegate::NinePatchThumbAperture() const {
  DCHECK(theme_.UsesNinePatchThumbResource());
  return theme_.NinePatchThumbAperture(*scrollbar_);
}

bool ScrollbarLayerDelegate::ShouldPaint() const {
  // TODO(crbug.com/860499): Remove this condition, it should not occur.
  // Layers may exist and be painted for a |scrollbar_| that has had its
  // ScrollableArea detached. This seems weird because if the area is detached
  // the layer should be destroyed but here we are. https://crbug.com/860499.
  if (!scrollbar_->GetScrollableArea())
    return false;
  // When the frame is throttled, the scrollbar will not be painted because
  // the frame has not had its lifecycle updated. Thus the actual value of
  // HasTickmarks can't be known and may change once the frame is unthrottled.
  if (scrollbar_->GetScrollableArea()->IsThrottled())
    return false;
  return true;
}

bool ScrollbarLayerDelegate::HasTickmarks() const {
  return ShouldPaint() && scrollbar_->HasTickmarks();
}

void ScrollbarLayerDelegate::PaintPart(cc::PaintCanvas* canvas,
                                       cc::ScrollbarPart part) {
  if (!ShouldPaint())
    return;

  ScopedScrollbarPainter painter(*canvas, device_scale_factor_);
  // The canvas coordinate space is relative to the part's origin.
  switch (part) {
    case cc::THUMB: {
      IntRect rect(IntPoint(),
                   UsesNinePatchThumbResource()
                       ? theme_.NinePatchThumbCanvasSize(*scrollbar_)
                       : theme_.ThumbRect(*scrollbar_).Size());
      theme_.PaintThumb(painter.Context(), *scrollbar_, rect);
      scrollbar_->ClearThumbNeedsRepaint();
      break;
    }
    case cc::TRACK: {
      theme_.PaintTrackAndButtonsForCompositor(painter.Context(), *scrollbar_);
      theme_.PaintTickmarks(painter.Context(), *scrollbar_,
                            IntRect(TrackRect()));
      scrollbar_->ClearTrackNeedsRepaint();
      break;
    }
    case cc::TICKMARKS: {
      IntRect rect(IntPoint(), theme_.TrackRect(*scrollbar_).Size());
      theme_.PaintTickmarks(painter.Context(), *scrollbar_, rect);
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace blink
