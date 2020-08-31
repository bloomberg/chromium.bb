// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/test_display_layout_manager.h"

#include <utility>

#include "ui/display/types/display_snapshot.h"

namespace display {
namespace test {

TestDisplayLayoutManager::TestDisplayLayoutManager(
    std::vector<std::unique_ptr<DisplaySnapshot>> displays,
    MultipleDisplayState display_state)
    : displays_(std::move(displays)), display_state_(display_state) {}

TestDisplayLayoutManager::~TestDisplayLayoutManager() {}

DisplayConfigurator::StateController*
TestDisplayLayoutManager::GetStateController() const {
  return nullptr;
}

DisplayConfigurator::SoftwareMirroringController*
TestDisplayLayoutManager::GetSoftwareMirroringController() const {
  return nullptr;
}

MultipleDisplayState TestDisplayLayoutManager::GetDisplayState() const {
  return display_state_;
}

chromeos::DisplayPowerState TestDisplayLayoutManager::GetPowerState() const {
  NOTREACHED();
  return chromeos::DISPLAY_POWER_ALL_ON;
}

bool TestDisplayLayoutManager::GetDisplayLayout(
    const std::vector<DisplaySnapshot*>& displays,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    std::vector<DisplayConfigureRequest>* requests) const {
  NOTREACHED();
  return false;
}

std::vector<DisplaySnapshot*> TestDisplayLayoutManager::GetDisplayStates()
    const {
  std::vector<DisplaySnapshot*> snapshots(displays_.size());
  std::transform(
      displays_.cbegin(), displays_.cend(), snapshots.begin(),
      [](const std::unique_ptr<DisplaySnapshot>& item) { return item.get(); });
  return snapshots;
}

bool TestDisplayLayoutManager::IsMirroring() const {
  return display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR;
}

}  // namespace test
}  // namespace display
