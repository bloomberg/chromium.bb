// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"

#include "ash/shell.h"

namespace ash {

TabletModeControllerTestApi::TabletModeControllerTestApi()
    : tablet_mode_controller_(Shell::Get()->tablet_mode_controller()) {}

TabletModeControllerTestApi::~TabletModeControllerTestApi() = default;

void TabletModeControllerTestApi::EnterTabletMode() {
  tablet_mode_controller_->AttemptEnterTabletMode();
}

void TabletModeControllerTestApi::LeaveTabletMode(
    bool called_by_device_update) {
  tablet_mode_controller_->AttemptLeaveTabletMode(called_by_device_update);
}

}  // namespace ash