// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_TOGGLE_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_TOGGLE_CONTROLLER_H_

#include "ash/system/unified/unified_slider_view.h"

namespace ash {

class UnifiedSystemTrayModel;

// Controller of a toast showing enable/disable of keyboard backlight.
class KeyboardBacklightToggleController : public UnifiedSliderListener {
 public:
  explicit KeyboardBacklightToggleController(UnifiedSystemTrayModel* model);

  KeyboardBacklightToggleController(const KeyboardBacklightToggleController&) =
      delete;
  KeyboardBacklightToggleController& operator=(
      const KeyboardBacklightToggleController&) = delete;

  ~KeyboardBacklightToggleController() override;

  // UnifiedSliderListener:
  views::View* CreateView() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  UnifiedSystemTrayModel* const model_;
  UnifiedSliderView* slider_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_TOGGLE_CONTROLLER_H_
