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

// The length of the expand animation.
const int kExpandDurationMs = 120;

// How long we should wait before hiding the scrollbar.
const int kScrollbarHideTimeoutMs = 500;

// How many frames per second to target for the expand animation.
const int kExpandFrameRateHz = 60;

// The thickness of the normal and expanded scrollbars.
const int kScrollbarThickness = 12;
const int kExpandedScrollbarThickness = 16;

// The thickness of the scrollbar thumb.
const int kScrollbarThumbThickness = 8;

// The width of the scroller track border.
const int kScrollerTrackBorderWidth = 1;

// The amount the thumb is inset from both the ends and the sides of the normal
// and expanded tracks.
const int kScrollbarThumbInset = 2;
const int kExpandedScrollbarThumbInset = 3;

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

}  // namespace

//////////////////////////////////////////////////////////////////
// CocoaScrollBarThumb

class CocoaScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit CocoaScrollBarThumb(CocoaScrollBar* scroll_bar);
  ~CocoaScrollBarThumb() override;

  // Returns true if the thumb is in hovered state.
  bool IsStateHovered() const;

  // Returns true if the thumb is in pressed state.
  bool IsStatePressed() const;

 protected:
  // View:
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  // Converts scroll_bar() into a CocoaScrollBar object and returns it.
  CocoaScrollBar* cocoa_scroll_bar() {
    return static_cast<CocoaScrollBar*>(scroll_bar());
  }

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

bool CocoaScrollBarThumb::IsStateHovered() const {
  return GetState() == CustomButton::STATE_HOVERED;
}

bool CocoaScrollBarThumb::IsStatePressed() const {
  return GetState() == CustomButton::STATE_PRESSED;
}

gfx::Size CocoaScrollBarThumb::GetPreferredSize() const {
  return gfx::Size(kScrollbarThumbThickness, kScrollbarThumbThickness);
}

void CocoaScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  SkColor thumb_color = kScrollerDefaultThumbColor;
  if (cocoa_scroll_bar()->GetScrollerStyle() == NSScrollerStyleOverlay ||
      IsStateHovered() ||
      IsStatePressed()) {
    thumb_color = kScrollerHoverThumbColor;
  }

  gfx::Rect local_bounds(GetLocalBounds());
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(thumb_color);
  const SkScalar radius =
      std::min(local_bounds.width(), local_bounds.height());
  canvas->DrawRoundRect(local_bounds, radius, paint);
}

bool CocoaScrollBarThumb::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the mouse press if the scrollbar is hidden.
  if (cocoa_scroll_bar()->IsScrollbarFullyHidden())
    return false;

  return BaseScrollBarThumb::OnMousePressed(event);
}

void CocoaScrollBarThumb::OnMouseReleased(const ui::MouseEvent& event) {
  BaseScrollBarThumb::OnMouseReleased(event);
  scroll_bar()->OnMouseReleased(event);
}

void CocoaScrollBarThumb::OnMouseEntered(const ui::MouseEvent& event) {
  BaseScrollBarThumb::OnMouseEntered(event);
  scroll_bar()->OnMouseEntered(event);
}

void CocoaScrollBarThumb::OnMouseExited(const ui::MouseEvent& event) {
  // The thumb should remain pressed when dragged, even if the mouse leaves
  // the scrollview. The thumb will be set back to its hover or normal state
  // when the mouse is released.
  if (GetState() != CustomButton::STATE_PRESSED)
    SetState(CustomButton::STATE_NORMAL);
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar class

CocoaScrollBar::CocoaScrollBar(bool horizontal)
    : BaseScrollBar(horizontal, new CocoaScrollBarThumb(this)),
      hide_scrollbar_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kScrollbarHideTimeoutMs),
          base::Bind(&CocoaScrollBar::HideScrollbar, base::Unretained(this)),
          false),
      expand_animation_(kExpandDurationMs, kExpandFrameRateHz, this),
      is_expanded_(false),
      did_start_dragging_(false) {
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

  int inset = kScrollbarThumbInset;
  if (is_expanded_) {
    inset = expand_animation_.
        CurrentValueBetween(kScrollbarThumbInset,
                            kExpandedScrollbarThumbInset);
  }
  local_bounds.Inset(inset, inset);

  gfx::Size track_size = local_bounds.size();
  track_size.SetToMax(GetThumb()->GetPreferredSize());
  local_bounds.set_size(track_size);
  return local_bounds;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, ScrollBar:

int CocoaScrollBar::GetLayoutSize() const {
  return scroller_style_ == NSScrollerStyleOverlay ? 0 : ScrollbarThickness();
}

int CocoaScrollBar::GetContentOverlapSize() const {
  return scroller_style_ == NSScrollerStyleLegacy ? 0 : ScrollbarThickness();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::View:

void CocoaScrollBar::Layout() {
  // Set the thickness of the thumb according to the track bounds.
  // The length of the thumb is set by BaseScrollBar::Update().
  gfx::Rect thumb_bounds(GetThumb()->bounds());
  gfx::Rect track_bounds(GetTrackBounds());
  if (IsHorizontal()) {
    GetThumb()->SetBounds(thumb_bounds.x(),
                          track_bounds.y(),
                          thumb_bounds.width(),
                          track_bounds.height());
  } else {
    GetThumb()->SetBounds(track_bounds.x(),
                          thumb_bounds.y(),
                          track_bounds.width(),
                          thumb_bounds.height());
  }
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

bool CocoaScrollBar::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the mouse press if the scrollbar is hidden.
  if (IsScrollbarFullyHidden())
    return false;

  return BaseScrollBar::OnMousePressed(event);
}

void CocoaScrollBar::OnMouseReleased(const ui::MouseEvent& event) {
  ResetOverlayScrollbar();
}

void CocoaScrollBar::OnMouseEntered(const ui::MouseEvent& event) {
  if (scroller_style_ != NSScrollerStyleOverlay)
    return;

  // If the scrollbar thumb did not completely fade away, then reshow it when
  // the mouse enters the scrollbar thumb.
  if (!IsScrollbarFullyHidden())
    ShowScrollbar();

  // Expand the scrollbar. If the scrollbar is hidden, don't animate it.
  if (!is_expanded_) {
    SetScrolltrackVisible(true);

    is_expanded_ = true;
    if (IsScrollbarFullyHidden()) {
      expand_animation_.SetCurrentValue(1.0);
      UpdateScrollbarThickness();
    } else {
      expand_animation_.Start();
    }
  }

  hide_scrollbar_timer_.Reset();
}

void CocoaScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  ResetOverlayScrollbar();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::BaseScrollBar:

void CocoaScrollBar::ScrollToPosition(int position) {
  BaseScrollBar::ScrollToPosition(position);

  if (GetCocoaScrollBarThumb()->IsStatePressed())
    did_start_dragging_ = true;

  if (scroller_style_ == NSScrollerStyleOverlay) {
    ShowScrollbar();
    hide_scrollbar_timer_.Reset();
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ViewsScrollbarBridge:

void CocoaScrollBar::OnScrollerStyleChanged() {
  NSScrollerStyle scroller_style =
      [ViewsScrollbarBridge getPreferredScrollerStyle];
  if (scroller_style_ == scroller_style)
    return;

  // Cancel all of the animations.
  expand_animation_.Stop();
  layer()->GetAnimator()->AbortAllAnimations();

  scroller_style_ = scroller_style;

  // Ensure that the ScrollView updates the scrollbar's layout.
  if (parent())
    parent()->Layout();

  if (scroller_style_ == NSScrollerStyleOverlay) {
    // Hide the scrollbar, but don't fade out.
    layer()->SetOpacity(0.0);
    ResetOverlayScrollbar();
  } else {
    is_expanded_ = false;
    SetScrolltrackVisible(true);
    ShowScrollbar();
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ImplicitAnimationObserver:

void CocoaScrollBar::OnImplicitAnimationsCompleted() {
  DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);
  ResetOverlayScrollbar();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::AnimationDelegate:

void CocoaScrollBar::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK(is_expanded_);
  UpdateScrollbarThickness();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, public:

bool CocoaScrollBar::IsScrollbarFullyHidden() const {
  return layer()->opacity() == 0.0;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, private:

// TODO(spqchan): Animate the scrollbar shrinking back to its original
// thickness when the scrollbar fades away so that it'll look less janky.
void CocoaScrollBar::HideScrollbar() {
  DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);

  // Don't disappear if the scrollbar is hovered, or pressed but not dragged.
  // This behavior matches the Cocoa scrollbars, but differs from the Blink
  // scrollbars which would just disappear.
  CocoaScrollBarThumb* thumb = GetCocoaScrollBarThumb();
  if (IsMouseHovered() || thumb->IsStateHovered() ||
      (thumb->IsStatePressed() && !did_start_dragging_)) {
    hide_scrollbar_timer_.Reset();
    return;
  }

  did_start_dragging_ = false;

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFadeDurationMs));
  animation.AddObserver(this);
  layer()->SetOpacity(0.0);
}

void CocoaScrollBar::ShowScrollbar() {
  // Updates the scrolltrack and repaint it, if necessary.
  layer()->SetOpacity(1.0);
  hide_scrollbar_timer_.Stop();
}

bool CocoaScrollBar::IsHoverOrPressedState() const {
  CocoaScrollBarThumb* thumb = GetCocoaScrollBarThumb();
  return thumb->IsStateHovered() ||
         thumb->IsStatePressed() ||
         IsMouseHovered();
}

void CocoaScrollBar::UpdateScrollbarThickness() {
  int thickness = ScrollbarThickness();
  if (IsHorizontal())
    SetBounds(x(), bounds().bottom() - thickness, width(), thickness);
  else
    SetBounds(bounds().right() - thickness, y(), thickness, height());
}

void CocoaScrollBar::ResetOverlayScrollbar() {
  if (!IsHoverOrPressedState() && IsScrollbarFullyHidden()) {
    if (is_expanded_) {
      is_expanded_ = false;
      expand_animation_.Stop();
      UpdateScrollbarThickness();
    }
    SetScrolltrackVisible(false);
  }
}

int CocoaScrollBar::ScrollbarThickness() const {
  if (scroller_style_ == NSScrollerStyleLegacy || !is_expanded_)
    return kScrollbarThickness;

  return expand_animation_.
      CurrentValueBetween(kScrollbarThickness, kExpandedScrollbarThickness);
}

void CocoaScrollBar::SetScrolltrackVisible(bool visible) {
  has_scrolltrack_ = visible;
  SchedulePaint();
}

CocoaScrollBarThumb* CocoaScrollBar::GetCocoaScrollBarThumb() const {
  return static_cast<CocoaScrollBarThumb*>(GetThumb());
}

}  // namespace views
