// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/touch_transform_controller_test_api.h"

namespace display {
namespace test {

TouchTransformControllerTestApi::TouchTransformControllerTestApi(
    TouchTransformController* controller)
    : controller_(controller) {}

TouchTransformControllerTestApi::~TouchTransformControllerTestApi() = default;

}  // namespace test
}  // namespace display
