// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

gfx::Point TestInkDropHost::CalculateInkDropCenter() const {
  return gfx::Point();
}

bool TestInkDropHost::ShouldShowInkDropHover() const {
  return should_show_hover_;
}

}  // namespace views
