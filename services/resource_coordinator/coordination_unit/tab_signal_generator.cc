// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/tab_signal_generator.h"

#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"

namespace resource_coordinator {

TabSignalGenerator::TabSignalGenerator() = default;

TabSignalGenerator::~TabSignalGenerator() = default;

bool TabSignalGenerator::ShouldObserve(
    const CoordinationUnitImpl* coordination_unit) {
  return coordination_unit->id().type == CoordinationUnitType::kWebContents;
}

}  // namespace resource_coordinator
