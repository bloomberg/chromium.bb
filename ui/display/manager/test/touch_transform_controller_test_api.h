// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_TOUCH_TRANSFORM_CONTROLLER_TEST_API_H_
#define UI_DISPLAY_MANAGER_TEST_TOUCH_TRANSFORM_CONTROLLER_TEST_API_H_

#include <memory>

#include "base/macros.h"
#include "ui/display/manager/touch_transform_controller.h"

namespace display {
namespace test {

class TouchTransformControllerTestApi {
 public:
  explicit TouchTransformControllerTestApi(
      TouchTransformController* controller);
  ~TouchTransformControllerTestApi();

  TouchTransformSetter* touch_transform_setter() {
    return controller_->touch_transform_setter_.get();
  }

 private:
  TouchTransformController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TouchTransformControllerTestApi);
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TEST_TOUCH_TRANSFORM_CONTROLLER_TEST_API_H_
