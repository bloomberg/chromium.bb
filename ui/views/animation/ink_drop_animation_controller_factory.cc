// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_factory.h"

#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/animation/ink_drop_animation_controller_impl.h"
#include "ui/views/views_export.h"

namespace views {

namespace {

// A stub implementation of an InkDropAnimationController that can be used when
// material design is not enabled.
class VIEWS_EXPORT InkDropAnimationControllerStub
    : public InkDropAnimationController {
 public:
  explicit InkDropAnimationControllerStub(InkDropHost* ink_drop_host);
  ~InkDropAnimationControllerStub() override;

  // InkDropAnimationController:
  void AnimateToState(InkDropState state) override;
  void SetInkDropSize(const gfx::Size& size) override;
  gfx::Rect GetInkDropBounds() const override;
  void SetInkDropBounds(const gfx::Rect& bounds) override;

 private:
  // The bounds of the ink drop layers. Defined in the coordinate space of the
  // parent ui::Layer that the ink drop layers were added to.
  gfx::Rect ink_drop_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerStub);
};

InkDropAnimationControllerStub::InkDropAnimationControllerStub(
    InkDropHost* ink_drop_host) {}

InkDropAnimationControllerStub::~InkDropAnimationControllerStub() {}

void InkDropAnimationControllerStub::AnimateToState(InkDropState state) {}

void InkDropAnimationControllerStub::SetInkDropSize(const gfx::Size& size) {
  ink_drop_bounds_.set_size(size);
}

gfx::Rect InkDropAnimationControllerStub::GetInkDropBounds() const {
  return ink_drop_bounds_;
}

void InkDropAnimationControllerStub::SetInkDropBounds(const gfx::Rect& bounds) {
  ink_drop_bounds_ = bounds;
}

}  // namespace

InkDropAnimationControllerFactory::InkDropAnimationControllerFactory() {}

InkDropAnimationControllerFactory::~InkDropAnimationControllerFactory() {}

scoped_ptr<InkDropAnimationController>
InkDropAnimationControllerFactory::CreateInkDropAnimationController(
    InkDropHost* ink_drop_host) {
#if defined(OS_CHROMEOS)
  if (ui::MaterialDesignController::IsModeMaterial()) {
    return scoped_ptr<InkDropAnimationController>(
        new InkDropAnimationControllerImpl(ink_drop_host));
  } else {
    return scoped_ptr<InkDropAnimationController>(
        new InkDropAnimationControllerStub(ink_drop_host));
  }
#endif  // defined(OS_CHROMEOS)

  return scoped_ptr<InkDropAnimationController>(
      new InkDropAnimationControllerStub(ink_drop_host));
}

}  // namespace views
