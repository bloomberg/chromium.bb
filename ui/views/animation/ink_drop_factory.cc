// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_factory.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/views_export.h"

namespace views {

namespace {

// A stub implementation of an InkDrop that can be used when material design is
// not enabled.
class InkDropStub : public InkDrop {
 public:
  InkDropStub() {}
  ~InkDropStub() override {}

  // InkDrop:
  InkDropState GetTargetInkDropState() const override {
    return InkDropState::HIDDEN;
  }
  bool IsVisible() const override { return false; }
  void AnimateToState(InkDropState state) override {}
  void SnapToActivated() override {}
  void SetHovered(bool is_hovered) override {}
  void SetFocused(bool is_hovered) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropStub);
};

}  // namespace

InkDropFactory::InkDropFactory() {}

InkDropFactory::~InkDropFactory() {}

std::unique_ptr<InkDrop> InkDropFactory::CreateInkDrop(
    InkDropHost* ink_drop_host) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    return base::WrapUnique(new InkDropImpl(ink_drop_host));
  }

  return base::WrapUnique(new InkDropStub());
}

}  // namespace views
