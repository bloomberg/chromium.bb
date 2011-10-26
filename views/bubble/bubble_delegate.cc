// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_delegate.h"

#include "ui/base/animation/slide_animation.h"
#include "views/bubble/bubble_frame_view.h"
#include "views/widget/widget.h"

// The duration of the fade animation in milliseconds.
static const int kHideFadeDurationMS = 1000;

namespace views {

BubbleDelegateView::BubbleDelegateView()
    : WidgetDelegateView(),
      close_on_esc_(true),
      arrow_location_(BubbleBorder::TOP_LEFT),
      color_(SK_ColorWHITE) {
  AddAccelerator(Accelerator(ui::VKEY_ESCAPE, 0));
}

BubbleDelegateView::BubbleDelegateView(
    const gfx::Point& anchor_point,
    BubbleBorder::ArrowLocation arrow_location,
    const SkColor& color)
    : WidgetDelegateView(),
      close_on_esc_(true),
      anchor_point_(anchor_point),
      arrow_location_(arrow_location),
      color_(color) {
  AddAccelerator(Accelerator(ui::VKEY_ESCAPE, 0));
}

BubbleDelegateView::~BubbleDelegateView() {}

// static
Widget* BubbleDelegateView::CreateBubble(BubbleDelegateView* bubble_delegate,
                                         Widget* parent_widget) {
  bubble_delegate->Init();
  views::Widget* bubble_widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_BUBBLE);
  params.delegate = bubble_delegate;
  params.transparent = true;
  if (!parent_widget)
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent_widget = parent_widget;
  bubble_widget->Init(params);
  bubble_widget->SetBounds(bubble_delegate->GetBubbleBounds());
  return bubble_widget;
}

View* BubbleDelegateView::GetInitiallyFocusedView() {
  return this;
}

View* BubbleDelegateView::GetContentsView() {
  return this;
}

ClientView* BubbleDelegateView::CreateClientView(Widget* widget) {
  return new ClientView(widget, GetContentsView());
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView() {
  return new BubbleFrameView(GetArrowLocation(),
                             GetPreferredSize(),
                             GetColor());
}

gfx::Point BubbleDelegateView::GetAnchorPoint() {
  return anchor_point_;
}

BubbleBorder::ArrowLocation BubbleDelegateView::GetArrowLocation() const {
  return arrow_location_;
}

SkColor BubbleDelegateView::GetColor() const {
  return color_;
}

void BubbleDelegateView::Init() {}

void BubbleDelegateView::StartFade(bool fade_in) {
  fade_animation_.reset(new ui::SlideAnimation(this));
  fade_animation_->SetSlideDuration(kHideFadeDurationMS);
  fade_animation_->Reset(fade_in ? 0.0 : 1.0);
  if (fade_in) {
    GetWidget()->SetOpacity(0);
    GetWidget()->Show();
    fade_animation_->Show();
  } else {
    fade_animation_->Hide();
  }
}

bool BubbleDelegateView::AcceleratorPressed(const Accelerator& accelerator) {
  if (!close_on_esc() || accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  if (fade_animation_.get())
    fade_animation_->Reset();
  GetWidget()->Close();
  return true;
}

void BubbleDelegateView::AnimationEnded(const ui::Animation* animation) {
  DCHECK_EQ(animation, fade_animation_.get());
  bool closed = fade_animation_->GetCurrentValue() == 0;
  fade_animation_->Reset();
  if (closed)
    GetWidget()->Close();
}

void BubbleDelegateView::AnimationProgressed(const ui::Animation* animation) {
  DCHECK_EQ(animation, fade_animation_.get());
  DCHECK(fade_animation_->is_animating());
  GetWidget()->SetOpacity(fade_animation_->GetCurrentValue() * 255);
  SchedulePaint();
}

const BubbleView* BubbleDelegateView::GetBubbleView() const {
  return GetWidget()->client_view()->AsBubbleView();
}

const BubbleFrameView* BubbleDelegateView::GetBubbleFrameView() const {
  return static_cast<BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
}

gfx::Rect BubbleDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  return GetBubbleFrameView()->GetWindowBoundsForClientBounds(
      gfx::Rect(GetAnchorPoint(), GetPreferredSize()));
}

}  // namespace views
