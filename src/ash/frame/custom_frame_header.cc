// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/custom_frame_header.h"

#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/detached_title_area_renderer.h"
#include "ash/frame/frame_header_util.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/frame_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Creates a path with rounded top corners.
SkPath MakeRoundRectPath(const gfx::Rect& bounds,
                         int top_left_corner_radius,
                         int top_right_corner_radius) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar kTopLeftRadius = SkIntToScalar(top_left_corner_radius);
  const SkScalar kTopRightRadius = SkIntToScalar(top_right_corner_radius);
  SkScalar radii[8] = {kTopLeftRadius,
                       kTopLeftRadius,  // top-left
                       kTopRightRadius,
                       kTopRightRadius,  // top-right
                       0,
                       0,  // bottom-right
                       0,
                       0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  return path;
}

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& frame_image,
                                 const gfx::ImageSkia& frame_overlay_image,
                                 SkColor background_color,
                                 const gfx::Rect& bounds,
                                 int image_inset_x,
                                 int image_inset_y,
                                 int alpha,
                                 int corner_radius) {
  SkPath frame_path = MakeRoundRectPath(bounds, corner_radius, corner_radius);
  bool antialias = corner_radius > 0;

  gfx::ScopedCanvas scoped_save(canvas);
  canvas->ClipPath(frame_path, antialias);

  PaintThemedFrame(canvas, frame_image, frame_overlay_image, background_color,
                   bounds, image_inset_x, image_inset_y, alpha);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// CustomFrameHeader, public:

CustomFrameHeader::CustomFrameHeader(
    views::Widget* target_widget,
    views::View* view,
    AppearanceProvider* appearance_provider,
    FrameCaptionButtonContainerView* caption_button_container)
    : FrameHeader(target_widget, view) {
  DCHECK(appearance_provider);
  DCHECK(caption_button_container);
  appearance_provider_ = appearance_provider;

  SetCaptionButtonContainer(caption_button_container);
}

CustomFrameHeader::~CustomFrameHeader() = default;

///////////////////////////////////////////////////////////////////////////////
// CustomFrameHeader, protected:

void CustomFrameHeader::DoPaintHeader(gfx::Canvas* canvas) {
  // If this is a detached title area renderer (i.e. for client-controlled
  // immersive mode), then Mash only needs to render the window caption buttons.
  if (!DetachedTitleAreaRendererForClient::ForWindow(
          view()->GetWidget()->GetNativeView())) {
    PaintFrameImages(canvas, false /* active */);
    PaintFrameImages(canvas, true /* active */);
    PaintTitleBar(canvas);
  }
}

AshLayoutSize CustomFrameHeader::GetButtonLayoutSize() const {
  return target_widget()->IsMaximized() || target_widget()->IsFullscreen() ||
                 appearance_provider_->IsTabletMode()
             ? AshLayoutSize::kBrowserCaptionMaximized
             : AshLayoutSize::kBrowserCaptionRestored;
}

SkColor CustomFrameHeader::GetTitleColor() const {
  return appearance_provider_->GetTitleColor();
}

SkColor CustomFrameHeader::GetCurrentFrameColor() const {
  return appearance_provider_->GetFrameHeaderColor(mode() == MODE_ACTIVE);
}

void CustomFrameHeader::DoSetFrameColors(SkColor active_frame_color,
                                         SkColor inactive_frame_color) {
  UpdateCaptionButtonColors();
  view()->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameHeader, private:

void CustomFrameHeader::PaintFrameImages(gfx::Canvas* canvas, bool active) {
  int alpha = activation_animation().CurrentValueBetween(0, 0xFF);
  if (!active)
    alpha = 0xFF - alpha;

  if (alpha == 0)
    return;

  gfx::ImageSkia frame_image =
      appearance_provider_->GetFrameHeaderImage(active);
  gfx::ImageSkia frame_overlay_image =
      appearance_provider_->GetFrameHeaderOverlayImage(active);

  int corner_radius = 0;
  if (!target_widget()->IsMaximized() && !target_widget()->IsFullscreen())
    corner_radius = FrameHeaderUtil::GetTopCornerRadiusWhenRestored();

  PaintFrameImagesInRoundRect(
      canvas, frame_image, frame_overlay_image,
      appearance_provider_->GetFrameHeaderColor(active), GetPaintedBounds(),
      FrameHeaderUtil::GetThemeBackgroundXInset(),
      appearance_provider_->GetFrameHeaderImageYInset(), alpha, corner_radius);
}

}  // namespace ash
