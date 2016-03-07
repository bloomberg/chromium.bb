// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"

#import "base/mac/sdk_forward_declarations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"

namespace views {

namespace {

// The length of the fade animation.
const int kFadeDurationMs = 240;

// How long we should wait before hiding the scrollbar.
const int kScrollbarHideTimeoutMs = 500;

// The width of the scrollbar.
const int kScrollbarWidth = 15;

// The width of the scrollbar thumb.
const int kScrollbarThumbWidth = 10;

// The width of the scroller track border.
const int kScrollerTrackBorderWidth = 1;

// The amount the thumb is inset from both the ends and the sides of the track.
const int kScrollbarThumbInset = 3;

// Scrollbar thumb colors.
const SkColor kScrollerDefaultThumbColor = SkColorSetARGB(0x38, 0, 0, 0);
const SkColor kScrollerHoverThumbColor = SkColorSetARGB(0x80, 0, 0, 0);

// Scroller track colors.
// TODO(spqchan): Add an alpha channel for the overlay-style scroll track.
const SkColor kScrollerTrackGradientColors[] = {
    SkColorSetRGB(0xEF, 0xEF, 0xEF), SkColorSetRGB(0xF9, 0xF9, 0xF9),
    SkColorSetRGB(0xFD, 0xFD, 0xFD), SkColorSetRGB(0xF6, 0xF6, 0xF6)};
const SkColor kScrollerTrackInnerBorderColor = SkColorSetRGB(0xE4, 0xE4, 0xE4);
const SkColor kScrollerTrackOuterBorderColor = SkColorSetRGB(0xEF, 0xEF, 0xEF);

//////////////////////////////////////////////////////////////////
// CocoaScrollBarThumb

class CocoaScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit CocoaScrollBarThumb(CocoaScrollBar* scroll_bar);
  ~CocoaScrollBarThumb() override;

  // Returns true if the thumb is in hover or pressed state.
  bool IsStateHoverOrPressed();

 protected:
  // View:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CocoaScrollBarThumb);
};

CocoaScrollBarThumb::CocoaScrollBarThumb(CocoaScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar) {
  DCHECK(scroll_bar);

  // This is necessary, otherwise the thumb will be rendered below the views if
  // those views paint to their own layers.
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
}

CocoaScrollBarThumb::~CocoaScrollBarThumb() {}

bool CocoaScrollBarThumb::IsStateHoverOrPressed() {
  CustomButton::ButtonState state = GetState();
  return state == CustomButton::STATE_HOVERED ||
         state == CustomButton::STATE_PRESSED;
}

gfx::Size CocoaScrollBarThumb::GetPreferredSize() const {
  return gfx::Size(kScrollbarThumbWidth, kScrollbarThumbWidth);
}

void CocoaScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  CocoaScrollBar* scrollbar = static_cast<CocoaScrollBar*>(scroll_bar());
  DCHECK(scrollbar);

  SkColor thumb_color = kScrollerDefaultThumbColor;
  if (scrollbar->GetScrollerStyle() == NSScrollerStyleOverlay ||
      IsStateHoverOrPressed()) {
    thumb_color = kScrollerHoverThumbColor;
  }

  gfx::Rect local_bounds(GetLocalBounds());
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(thumb_color);
  const SkScalar radius = std::min(local_bounds.width(), local_bounds.height());
  canvas->DrawRoundRect(local_bounds, radius, paint);
}

void CocoaScrollBarThumb::OnMouseEntered(const ui::MouseEvent& event) {
  BaseScrollBarThumb::OnMouseEntered(event);
  CocoaScrollBar* scrollbar = static_cast<CocoaScrollBar*>(scroll_bar());
  scrollbar->OnMouseEnteredScrollbarThumb(event);
}

}  // namespace

//////////////////////////////////////////////////////////////////
// CocoaScrollBar class

CocoaScrollBar::CocoaScrollBar(bool horizontal)
    : BaseScrollBar(horizontal, new CocoaScrollBarThumb(this)),
      hide_scrollbar_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kScrollbarHideTimeoutMs),
          base::Bind(&CocoaScrollBar::HideScrollbar, base::Unretained(this)),
          false) {
  bridge_.reset([[ViewsScrollbarBridge alloc] initWithDelegate:this]);

  scroller_style_ = [ViewsScrollbarBridge getPreferredScrollerStyle];

  SetPaintToLayer(true);
  has_scrolltrack_ = scroller_style_ == NSScrollerStyleLegacy;
  layer()->SetOpacity(scroller_style_ == NSScrollerStyleOverlay ? 0.0 : 1.0);
}

CocoaScrollBar::~CocoaScrollBar() {
  [bridge_ clearDelegate];
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, BaseScrollBar:

gfx::Rect CocoaScrollBar::GetTrackBounds() const {
  gfx::Rect local_bounds(GetLocalBounds());
  local_bounds.Inset(kScrollbarThumbInset, kScrollbarThumbInset);

  gfx::Size track_size = local_bounds.size();
  track_size.SetToMax(GetThumb()->size());
  local_bounds.set_size(track_size);
  return local_bounds;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, ScrollBar:

int CocoaScrollBar::GetLayoutSize() const {
  return scroller_style_ == NSScrollerStyleOverlay ? 0 : kScrollbarWidth;
}

int CocoaScrollBar::GetContentOverlapSize() const {
  return scroller_style_ == NSScrollerStyleLegacy ? 0 : kScrollbarWidth;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::Views:

void CocoaScrollBar::Layout() {
  GetThumb()->SetBoundsRect(GetTrackBounds());
}

gfx::Size CocoaScrollBar::GetPreferredSize() const {
  return gfx::Size();
}

void CocoaScrollBar::OnPaint(gfx::Canvas* canvas) {
  if (!has_scrolltrack_)
    return;

  // Paint the scrollbar track background.
  gfx::Rect track_rect = GetLocalBounds();

  SkPoint gradient_bounds[2];
  if (IsHorizontal()) {
    gradient_bounds[0].set(track_rect.x(), track_rect.y());
    gradient_bounds[1].set(track_rect.x(), track_rect.bottom());
  } else {
    gradient_bounds[0].set(track_rect.x(), track_rect.y());
    gradient_bounds[1].set(track_rect.right(), track_rect.y());
  }
  skia::RefPtr<SkShader> shader = skia::AdoptRef(SkGradientShader::CreateLinear(
      gradient_bounds, kScrollerTrackGradientColors, nullptr,
      arraysize(kScrollerTrackGradientColors), SkShader::kClamp_TileMode));
  SkPaint gradient;
  gradient.setShader(shader.get());
  canvas->DrawRect(track_rect, gradient);

  // Draw the inner border: top if horizontal, left if vertical.
  SkPaint paint;
  paint.setColor(kScrollerTrackInnerBorderColor);
  gfx::Rect inner_border(track_rect);
  if (IsHorizontal())
    inner_border.set_height(kScrollerTrackBorderWidth);
  else
    inner_border.set_width(kScrollerTrackBorderWidth);
  canvas->DrawRect(inner_border, paint);

  // Draw the outer border: bottom if horizontal, right if veritcal.
  paint.setColor(kScrollerTrackOuterBorderColor);
  gfx::Rect outer_border(inner_border);
  if (IsHorizontal())
    outer_border.set_y(track_rect.bottom());
  else
    outer_border.set_x(track_rect.right());
  canvas->DrawRect(outer_border, paint);
}

void CocoaScrollBar::OnMouseEnteredScrollbarThumb(const ui::MouseEvent& event) {
  if (scroller_style_ != NSScrollerStyleOverlay)
    return;

  // If the scrollbar thumb has not compeletely faded away, then reshow it when
  // the mouse enters the scrollbar thumb.
  if (layer()->opacity())
    ShowScrollbar();

  hide_scrollbar_timer_.Reset();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ScrollDelegate:

bool CocoaScrollBar::OnScroll(float dx, float dy) {
  bool did_scroll = BaseScrollBar::OnScroll(dx, dy);
  if (did_scroll && scroller_style_ == NSScrollerStyleOverlay) {
    ShowScrollbar();
    hide_scrollbar_timer_.Reset();
  }
  return did_scroll;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ViewsScrollbarBridge:

void CocoaScrollBar::OnScrollerStyleChanged() {
  NSScrollerStyle scroller_style =
      [ViewsScrollbarBridge getPreferredScrollerStyle];
  if (scroller_style_ == scroller_style)
    return;

  scroller_style_ = scroller_style;

  // Ensure that the ScrollView updates the scrollbar's layout.
  if (parent())
    parent()->Layout();

  if (scroller_style_ == NSScrollerStyleOverlay) {
    // Hide the scrollbar, but don't fade out.
    layer()->SetOpacity(0.0);
  } else {
    ShowScrollbar();
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, private:

void CocoaScrollBar::HideScrollbar() {
  DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);

  // If the thumb is in a hover or pressed state, we don't want the scrollbar
  // to disappear. As such, we should reset the timer.
  CocoaScrollBarThumb* thumb = static_cast<CocoaScrollBarThumb*>(GetThumb());
  if (thumb->IsStateHoverOrPressed()) {
    hide_scrollbar_timer_.Reset();
    return;
  }

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFadeDurationMs));
  layer()->SetOpacity(0.0);
}

void CocoaScrollBar::ShowScrollbar() {
  // Updates the scrolltrack and repaint it, if necessary.
  CocoaScrollBarThumb* thumb = static_cast<CocoaScrollBarThumb*>(GetThumb());
  bool has_scrolltrack = scroller_style_ == NSScrollerStyleLegacy ||
                         thumb->IsStateHoverOrPressed();
  if (has_scrolltrack_ != has_scrolltrack) {
    has_scrolltrack_ = has_scrolltrack;
    SchedulePaint();
  }

  layer()->SetOpacity(1.0);
  hide_scrollbar_timer_.Stop();
}

}  // namespace views
