// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/views_export.h"

namespace views {
class InkDropAnimation;
class InkDropHost;

// Controls an ink drop animation which is hosted by an InkDropHost.
class VIEWS_EXPORT InkDropAnimationControllerImpl
    : public InkDropAnimationController {
 public:
  // Constructs an ink drop controller that will attach the ink drop to the
  // given |ink_drop_host|.
  explicit InkDropAnimationControllerImpl(InkDropHost* ink_drop_host);
  ~InkDropAnimationControllerImpl() override;

  // InkDropAnimationController:
  void AnimateToState(InkDropState state) override;
  void SetInkDropSize(const gfx::Size& size) override;
  gfx::Rect GetInkDropBounds() const override;
  void SetInkDropBounds(const gfx::Rect& bounds) override;

 private:
  // The host of the ink drop.
  InkDropHost* ink_drop_host_;

  // TODO(bruthig): It will be expensive to maintain InkDropAnimation instances
  // when they are not actually being used. Consider creating InkDropAnimations
  // on an as-needed basis and if construction is also expensive then consider
  // creating an InkDropAnimationPool. See www.crbug.com/522175.
  scoped_ptr<InkDropAnimation> ink_drop_animation_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerImpl);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_
