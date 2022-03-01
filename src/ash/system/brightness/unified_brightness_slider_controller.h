// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_SLIDER_CONTROLLER_H_

#include "ash/system/unified/unified_slider_view.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

class UnifiedSystemTrayModel;

// Controller of a slider that can change display brightness.
class UnifiedBrightnessSliderController : public UnifiedSliderListener {
 public:
  explicit UnifiedBrightnessSliderController(
      scoped_refptr<UnifiedSystemTrayModel> model);

  UnifiedBrightnessSliderController(const UnifiedBrightnessSliderController&) =
      delete;
  UnifiedBrightnessSliderController& operator=(
      const UnifiedBrightnessSliderController&) = delete;

  ~UnifiedBrightnessSliderController() override;

  // UnifiedSliderListener:
  views::View* CreateView() override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
  UnifiedSliderView* slider_ = nullptr;

  // We have to store previous manually set value because |old_value| might be
  // set by UnifiedSystemTrayModel::Observer.
  double previous_percent_ = 100.0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_SLIDER_CONTROLLER_H_
