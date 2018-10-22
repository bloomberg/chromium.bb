// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"

namespace chromeos {
namespace power {
namespace ml {

// Interface to indicate whether an upcoming screen dim should go ahead based on
// whether user will remain inactive if screen is dimmed now.
class SmartDimModel {
 public:
  virtual ~SmartDimModel() = default;

  // Returns a prediction whether an upcoming dim should go ahead based on input
  // |features|.
  virtual UserActivityEvent::ModelPrediction ShouldDim(
      const UserActivityEvent::Features& features) = 0;
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_H_
