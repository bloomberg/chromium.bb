// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_factory.h"

#include "base/macros.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/animation/ink_drop_animation_controller_impl.h"
#include "ui/views/views_export.h"

namespace views {

namespace {

// A stub implementation of an InkDropAnimationController that can be used when
// material design is not enabled.
class InkDropAnimationControllerStub
    : public InkDropAnimationController {
 public:
  explicit InkDropAnimationControllerStub();
  ~InkDropAnimationControllerStub() override;

  // InkDropAnimationController:
  InkDropState GetTargetInkDropState() const override;
  bool IsVisible() const override;
  void AnimateToState(InkDropState state) override;
  void SnapToActivated() override;
  void SetHovered(bool is_hovered) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerStub);
};

InkDropAnimationControllerStub::InkDropAnimationControllerStub() {}

InkDropAnimationControllerStub::~InkDropAnimationControllerStub() {}

InkDropState InkDropAnimationControllerStub::GetTargetInkDropState() const {
  return InkDropState::HIDDEN;
}

bool InkDropAnimationControllerStub::IsVisible() const {
  return false;
}

void InkDropAnimationControllerStub::AnimateToState(InkDropState state) {}

void InkDropAnimationControllerStub::SnapToActivated() {}

void InkDropAnimationControllerStub::SetHovered(bool is_hovered) {}

}  // namespace

InkDropAnimationControllerFactory::InkDropAnimationControllerFactory() {}

InkDropAnimationControllerFactory::~InkDropAnimationControllerFactory() {}

scoped_ptr<InkDropAnimationController>
InkDropAnimationControllerFactory::CreateInkDropAnimationController(
    InkDropHost* ink_drop_host) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    return scoped_ptr<InkDropAnimationController>(
        new InkDropAnimationControllerImpl(ink_drop_host));
  }

  return scoped_ptr<InkDropAnimationController>(
      new InkDropAnimationControllerStub());
}

}  // namespace views
