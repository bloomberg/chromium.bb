// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/views_export.h"

namespace views {
class InkDropAnimation;
class InkDropHost;

// A functional implementation of an InkDropAnimationController.
class VIEWS_EXPORT InkDropAnimationControllerImpl
    : public InkDropAnimationController,
      public InkDropAnimationObserver {
 public:
  // Constructs an ink drop controller that will attach the ink drop to the
  // given |ink_drop_host|.
  explicit InkDropAnimationControllerImpl(InkDropHost* ink_drop_host);
  ~InkDropAnimationControllerImpl() override;

  // InkDropAnimationController:
  InkDropState GetInkDropState() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  gfx::Size GetInkDropLargeSize() const override;
  void SetInkDropSize(const gfx::Size& large_size,
                      int large_corner_radius,
                      const gfx::Size& small_size,
                      int small_corner_radius) override;
  void SetInkDropCenter(const gfx::Point& center_point) override;

 private:
  // Creates a new InkDropAnimation and sets it to |ink_drop_animation_|. If
  // |ink_drop_animation_| wasn't null then it will be destroyed using
  // DestroyInkDropAnimation().
  void CreateInkDropAnimation();

  // Destroys the current |ink_drop_animation_|.
  void DestroyInkDropAnimation();

  // views::InkDropAnimationObserver:
  void InkDropAnimationStarted(InkDropState ink_drop_state) override;
  void InkDropAnimationEnded(InkDropState ink_drop_state,
                             InkDropAnimationEndedReason reason) override;

  // The host of the ink drop.
  InkDropHost* ink_drop_host_;

  // Cached size for the ink drop's large size animations.
  gfx::Size ink_drop_large_size_;

  // Cached corner radius for the ink drop's large size animations.
  int ink_drop_large_corner_radius_;

  // Cached size for the ink drop's small size animations.
  gfx::Size ink_drop_small_size_;

  // Cached corner radius for the ink drop's small size animations.
  int ink_drop_small_corner_radius_;

  // Cached center point for the ink drop.
  gfx::Point ink_drop_center_;

  // The current InkDropAnimation. Created on demand using
  // CreateInkDropAnimation().
  scoped_ptr<InkDropAnimation> ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerImpl);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_
