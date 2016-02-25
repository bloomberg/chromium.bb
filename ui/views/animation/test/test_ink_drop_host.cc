// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/square_ink_drop_animation.h"
#include "ui/views/animation/test/test_ink_drop_host.h"

namespace views {

TestInkDropHost::TestInkDropHost()
    : num_ink_drop_layers_(0), should_show_hover_(false) {}

TestInkDropHost::~TestInkDropHost() {}

void TestInkDropHost::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  ++num_ink_drop_layers_;
}

void TestInkDropHost::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  --num_ink_drop_layers_;
}

scoped_ptr<InkDropAnimation> TestInkDropHost::CreateInkDropAnimation() const {
  gfx::Size size(10, 10);
  return make_scoped_ptr(new SquareInkDropAnimation(
      size, 5, size, 5, gfx::Point(), SK_ColorBLACK));
}

scoped_ptr<InkDropHover> TestInkDropHost::CreateInkDropHover() const {
  return should_show_hover_
             ? make_scoped_ptr(new InkDropHover(gfx::Size(10, 10), 4,
                                                gfx::Point(), SK_ColorBLACK))
             : nullptr;
}

}  // namespace views
