// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_slider.h"

#include "ui/views/controls/slider.h"

namespace views {

// Arbitrary thumb size used for tests.
const int thumb_size_ = 10;

TestSlider::TestSlider(SliderListener* listener) : Slider(listener) {}

TestSlider::~TestSlider() {}

void TestSlider::UpdateState(bool control_on) {
}

const char* TestSlider::GetClassName() const {
  return "TestSlider";
}

int TestSlider::GetThumbWidth() {
  return thumb_size_;
}

}  // namespace views
