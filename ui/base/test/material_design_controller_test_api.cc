// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/material_design_controller_test_api.h"

namespace ui {
namespace test {

void MaterialDesignControllerTestAPI::SetMode(
    MaterialDesignController::Mode mode) {
  MaterialDesignController::SetMode(mode);
}

void MaterialDesignControllerTestAPI::UninitializeMode() {
  MaterialDesignController::UninitializeMode();
}

}  // namespace test
}  // namespace ui
