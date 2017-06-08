// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"

#import "base/mac/sdk_forward_declarations.h"
#include "cc/paint/paint_shader.h"
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
const int kExpandDurationMs = 240;

// How long we should wait before hiding the scrollbar.
const int kScrollbarHideTimeoutMs = 500;

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

// Opacity of the overlay scrollbar.
const float kOverlayOpacity = 0.8f;

// Scroller track colors.
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
  gfx::Size CalculatePreferredSize() const override;
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
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

CocoaScrollBarThumb::~CocoaScrollBarThumb() {}

bool CocoaScrollBarThumb::IsStateHovered() const {
  return GetState() == CustomButton::STATE_HOVERED;
}

bool CocoaScrollBarThumb::IsStatePressed() const {
  return GetState() == CustomButton::STATE_PRESSED;
}

gfx::Size CocoaScrollBarThumb::CalculatePreferredSize() const {
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
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(thumb_color);
  const SkScalar radius =
      std::min(local_bounds.width(), local_bounds.height());
  canvas->DrawRoundRect(local_bounds, radius, flags);
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
    : BaseScrollBar(horizontal),
      hide_scrollbar_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kScrollbarHideTimeoutMs),
          base::Bind(&CocoaScrollBar::HideScrollbar, base::Unretained(this)),
          false),
      thickness_animation_(this),
      last_contents_scroll_offset_(0),
      is_expanded_(false),
      did_start_dragging_(false) {
  SetThumb(new CocoaScrollBarThumb(this));
  bridge_.reset([[ViewsScrollbarBridge alloc] initWithDelegate:this]);
  scroller_style_ = [ViewsScrollbarBridge getPreferredScrollerStyle];

  thickness_animation_.SetSlideDuration(kExpandDurationMs);

  SetPaintToLayer();
  has_scrolltrack_ = scroller_style_ == NSScrollerStyleLegacy;
  layer()->SetOpacity(scroller_style_ == NSScrollerStyleOverlay ? 0.0f : 1.0f);
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
    inset = thickness_animation_.CurrentValueBetween(
        kScrollbarThumbInset, kExpandedScrollbarThumbInset);
  }
  local_bounds.Inset(inset, inset);

  gfx::Size track_size = local_bounds.size();
  track_size.SetToMax(GetThumb()->GetPreferredSize());
  local_bounds.set_size(track_size);
  return local_bounds;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, ScrollBar:

int CocoaScrollBar::GetThickness() const {
  return ScrollbarThickness();
}

bool CocoaScrollBar::OverlapsContent() const {
  return scroller_style_ == NSScrollerStyleOverlay;
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

gfx::Size CocoaScrollBar::CalculatePreferredSize() const {
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
  cc::PaintFlags gradient;
  gradient.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds, kScrollerTrackGradientColors, nullptr,
      arraysize(kScrollerTrackGradientColors), SkShader::kClamp_TileMode));
  canvas->DrawRect(track_rect, gradient);

  // Draw the inner border: top if horizontal, left if vertical.
  cc::PaintFlags flags;
  flags.setColor(kScrollerTrackInnerBorderColor);
  gfx::Rect inner_border(track_rect);
  if (IsHorizontal())
    inner_border.set_height(kScrollerTrackBorderWidth);
  else
    inner_border.set_width(kScrollerTrackBorderWidth);
  canvas->DrawRect(inner_border, flags);

  // Draw the outer border: bottom if horizontal, right if veritcal.
  flags.setColor(kScrollerTrackOuterBorderColor);
  gfx::Rect outer_border(inner_border);
  if (IsHorizontal())
    outer_border.set_y(track_rect.bottom());
  else
    outer_border.set_x(track_rect.right());
  canvas->DrawRect(outer_border, flags);
}

bool CocoaScrollBar::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the mouse press if the scrollbar is hidden.
  if (IsScrollbarFullyHidden())
    return false;

  return BaseScrollBar::OnMousePressed(event);
}

void CocoaScrollBar::OnMouseReleased(const ui::MouseEvent& event) {
  ResetOverlayScrollbar();
  BaseScrollBar::OnMouseReleased(event);
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
      thickness_animation_.Reset(1.0);
      UpdateScrollbarThickness();
    } else {
      thickness_animation_.Show();
    }
  }

  hide_scrollbar_timer_.Reset();
}

void CocoaScrollBar::OnMouseExited(const ui::MouseEvent& event) {
  ResetOverlayScrollbar();
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar::ScrollBar:

void CocoaScrollBar::Update(int viewport_size,
                            int content_size,
                            int contents_scroll_offset) {
  // TODO(tapted): Pass in overscroll amounts from the Layer and "Squish" the
  // scroller thumb accordingly.
  BaseScrollBar::Update(viewport_size, content_size, contents_scroll_offset);

  // Only reveal the scroller when |contents_scroll_offset| changes. Note this
  // is different to GetPosition() which can change due to layout. A layout
  // change can also change the offset; show the scroller in these cases. This
  // is consistent with WebContents (Cocoa will also show a scroller with any
  // mouse-initiated layout, but not programmatic size changes).
  if (contents_scroll_offset == last_contents_scroll_offset_)
    return;

  last_contents_scroll_offset_ = contents_scroll_offset;

  if (GetCocoaScrollBarThumb()->IsStatePressed())
    did_start_dragging_ = true;

  if (scroller_style_ == NSScrollerStyleOverlay) {
    ShowScrollbar();
    hide_scrollbar_timer_.Reset();
  }
}

void CocoaScrollBar::ObserveScrollEvent(const ui::ScrollEvent& event) {
  // Do nothing if the delayed hide timer is running. This means there has been
  // some recent scrolling in this direction already.
  if (scroller_style_ != NSScrollerStyleOverlay ||
      hide_scrollbar_timer_.IsRunning()) {
    return;
  }

  // Otherwise, when starting the event stream, show an overlay scrollbar to
  // indicate possible scroll directions, but do not start the hide timer.
  if (event.momentum_phase() == ui::EventMomentumPhase::MAY_BEGIN) {
    // Show only if the direction isn't yet known.
    if (event.x_offset() == 0 && event.y_offset() == 0)
      ShowScrollbar();
    return;
  }

  // If the direction matches, do nothing. This is needed in addition to the
  // hide timer check because Update() is called asynchronously, after event
  // processing. So when |event| is the first event in a particular direction
  // the hide timer will not have started.
  if ((IsHorizontal() ? event.x_offset() : event.y_offset()) != 0)
    return;

  // Otherwise, scrolling has started, but not in this scroller direction. If
  // already faded out, don't start another fade animation since that would
  // immediately finish the first fade animation.
  if (layer()->GetTargetOpacity() != 0) {
    // If canceling rather than picking a direction, fade out after a delay.
    if (event.momentum_phase() == ui::EventMomentumPhase::END)
      hide_scrollbar_timer_.Reset();
    else
      HideScrollbar();  // Fade out immediately.
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
  thickness_animation_.Reset();
  layer()->GetAnimator()->AbortAllAnimations();

  scroller_style_ = scroller_style;

  // Ensure that the ScrollView updates the scrollbar's layout.
  if (parent())
    parent()->Layout();

  if (scroller_style_ == NSScrollerStyleOverlay) {
    // Hide the scrollbar, but don't fade out.
    layer()->SetOpacity(0.0f);
    ResetOverlayScrollbar();
    GetThumb()->SchedulePaint();
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

void CocoaScrollBar::AnimationEnded(const gfx::Animation* animation) {
  // Remove the scrolltrack and set |is_expanded| to false at the end of
  // the shrink animation.
  if (!thickness_animation_.IsShowing()) {
    is_expanded_ = false;
    SetScrolltrackVisible(false);
  }
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, public:

bool CocoaScrollBar::IsScrollbarFullyHidden() const {
  return layer()->opacity() == 0.0f;
}

//////////////////////////////////////////////////////////////////
// CocoaScrollBar, private:

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
  layer()->SetOpacity(0.0f);
}

void CocoaScrollBar::ShowScrollbar() {
  // If the scrollbar is still expanded but has not completely faded away,
  // then shrink it back to its original state.
  if (is_expanded_ && !IsHoverOrPressedState() &&
      layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::OPACITY)) {
    DCHECK_EQ(scroller_style_, NSScrollerStyleOverlay);
    thickness_animation_.Hide();
  }

  // Updates the scrolltrack and repaint it, if necessary.
  double opacity =
      scroller_style_ == NSScrollerStyleOverlay ? kOverlayOpacity : 1.0f;
  layer()->SetOpacity(opacity);
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
  if (!IsHoverOrPressedState() && IsScrollbarFullyHidden() &&
      !thickness_animation_.IsClosing()) {
    if (is_expanded_) {
      is_expanded_ = false;
      thickness_animation_.Reset();
      UpdateScrollbarThickness();
    }
    SetScrolltrackVisible(false);
  }
}

int CocoaScrollBar::ScrollbarThickness() const {
  if (scroller_style_ == NSScrollerStyleLegacy)
    return kScrollbarThickness;

  return thickness_animation_.CurrentValueBetween(kScrollbarThickness,
                                                  kExpandedScrollbarThickness);
}

void CocoaScrollBar::SetScrolltrackVisible(bool visible) {
  has_scrolltrack_ = visible;
  SchedulePaint();
}

CocoaScrollBarThumb* CocoaScrollBar::GetCocoaScrollBarThumb() const {
  return static_cast<CocoaScrollBarThumb*>(GetThumb());
}

// static
base::Timer* BaseScrollBar::GetHideTimerForTest(BaseScrollBar* scroll_bar) {
  return &static_cast<CocoaScrollBar*>(scroll_bar)->hide_scrollbar_timer_;
}

}  // namespace views
