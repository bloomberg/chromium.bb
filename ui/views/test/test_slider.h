// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_SLIDER_H_
#define UI_VIEWS_TEST_TEST_SLIDER_H_

#include "base/macros.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {
class Slider;

// It is a dummy implementation of slider used for testing functionalities in
// slider.
class TestSlider : public Slider {
 public:
  explicit TestSlider(SliderListener* listener);
  ~TestSlider() override;

  // ui::Slider:
  void UpdateState(bool control_on) override;

  // views::View:
  const char* GetClassName() const override;

 protected:
  // ui::Slider:
  int GetThumbWidth() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSlider);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_SLIDER_H_
