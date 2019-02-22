// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/animated_image_view.h"

#include <utility>

#include "base/logging.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skottie_wrapper.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

bool AreAnimatedImagesEqual(const gfx::SkiaVectorAnimation& animation_1,
                            const gfx::SkiaVectorAnimation& animation_2) {
  // In rare cases this may return false, even if the animated images are backed
  // by the same resource file.
  return animation_1.skottie() == animation_2.skottie();
}

}  // namespace

AnimatedImageView::AnimatedImageView() = default;

AnimatedImageView::~AnimatedImageView() = default;

void AnimatedImageView::SetAnimatedImage(
    std::unique_ptr<gfx::SkiaVectorAnimation> animated_image) {
  if (animated_image_ &&
      AreAnimatedImagesEqual(*animated_image, *animated_image_)) {
    Stop();
    return;
  }

  gfx::Size preferred_size(GetPreferredSize());
  animated_image_ = std::move(animated_image);

  // Stop the animation to reset it.
  Stop();

  if (preferred_size != GetPreferredSize())
    PreferredSizeChanged();
  SchedulePaint();
}

void AnimatedImageView::Play() {
  DCHECK(animated_image_);
  DCHECK_EQ(state_, State::kStopped);

  state_ = State::kPlaying;

  // We cannot play the animation unless we have a valid compositor.
  if (!compositor_)
    return;

  // Ensure the class is added as an observer to receive clock ticks.
  if (!compositor_->HasAnimationObserver(this))
    compositor_->AddAnimationObserver(this);

  animated_image_->Start();
}

void AnimatedImageView::Stop() {
  DCHECK(animated_image_);
  if (compositor_)
    compositor_->RemoveAnimationObserver(this);

  animated_image_->Stop();
  state_ = State::kStopped;
}

gfx::Size AnimatedImageView::GetImageSize() const {
  return image_size_.value_or(
      animated_image_ ? animated_image_->GetOriginalSize() : gfx::Size());
}

void AnimatedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (!animated_image_)
    return;
  canvas->Save();
  canvas->Translate(GetImageBounds().origin().OffsetFromOrigin());

  // OnPaint may be called before clock tick was received; in that case just
  // paint the first frame.
  if (!previous_timestamp_.is_null() && state_ != State::kStopped)
    animated_image_->Paint(canvas, previous_timestamp_, GetImageSize());
  else
    animated_image_->PaintFrame(canvas, 0, GetImageSize());

  canvas->Restore();
}

const char* AnimatedImageView::GetClassName() const {
  return "AnimatedImageView";
}

void AnimatedImageView::NativeViewHierarchyChanged() {
  // When switching a window from one display to another, the compositor
  // associated with the widget changes.
  AddedToWidget();
}

void AnimatedImageView::AddedToWidget() {
  ui::Compositor* compositor = GetWidget()->GetCompositor();
  DCHECK(compositor);
  if (compositor_ != compositor) {
    if (compositor_ && compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = compositor;
  }
}

void AnimatedImageView::RemovedFromWidget() {
  if (compositor_) {
    Stop();
    if (compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

void AnimatedImageView::OnAnimationStep(base::TimeTicks timestamp) {
  previous_timestamp_ = timestamp;
  SchedulePaint();
}

void AnimatedImageView::OnCompositingShuttingDown(ui::Compositor* compositor) {
  if (compositor_ == compositor) {
    Stop();
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

}  // namespace views
