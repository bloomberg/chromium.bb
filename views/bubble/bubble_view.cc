// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_view.h"

#include "ui/base/animation/slide_animation.h"
#include "views/bubble/bubble_border.h"
#include "views/widget/widget.h"

// The duration of the fade animation in milliseconds.
static const int kHideFadeDurationMS = 1000;

namespace views {

BubbleView::BubbleView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      close_on_esc_(true) {
  AddAccelerator(Accelerator(ui::VKEY_ESCAPE, 0));
}

BubbleView::~BubbleView() {}

BubbleView* BubbleView::AsBubbleView() {
  return this;
}

const BubbleView* BubbleView::AsBubbleView() const {
  return this;
}

void BubbleView::StartFade(bool fade_in) {
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

bool BubbleView::AcceleratorPressed(const Accelerator& accelerator) {
  if (!close_on_esc_ || accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  if (fade_animation_.get())
    fade_animation_->Reset();
  GetWidget()->Close();
  return true;
}

void BubbleView::AnimationEnded(const ui::Animation* animation) {
  DCHECK_EQ(animation, fade_animation_.get());
  bool closed = fade_animation_->GetCurrentValue() == 0;
  fade_animation_->Reset();
  if (closed)
    GetWidget()->Close();
}

void BubbleView::AnimationProgressed(const ui::Animation* animation) {
  DCHECK_EQ(animation, fade_animation_.get());
  DCHECK(fade_animation_->is_animating());
  GetWidget()->SetOpacity(fade_animation_->GetCurrentValue() * 255);
  SchedulePaint();
}

}  // namespace views
