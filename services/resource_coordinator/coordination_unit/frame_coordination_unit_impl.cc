// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"

namespace resource_coordinator {

FrameCoordinationUnitImpl::FrameCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitImpl(id, std::move(service_ref)) {}

FrameCoordinationUnitImpl::~FrameCoordinationUnitImpl() = default;

std::set<CoordinationUnitImpl*>
FrameCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) {
  switch (type) {
    case CoordinationUnitType::kProcess:
    case CoordinationUnitType::kWebContents:
      // Processes and WebContents are only associated with a frame if
      // they are a direct parents of the frame.
      return GetParentCoordinationUnitsOfType(type);
    case CoordinationUnitType::kFrame: {
      // Frame association is recursive: any frame connected to a frame the
      // frame is connected to are all associated with one another.
      auto frame_coordination_units = GetChildCoordinationUnitsOfType(type);
      for (auto* frame_coordination_unit :
           GetParentCoordinationUnitsOfType(type)) {
        frame_coordination_units.insert(frame_coordination_unit);
      }
      return frame_coordination_units;
    }
    default:
      return std::set<CoordinationUnitImpl*>();
  }
}

bool FrameCoordinationUnitImpl::IsMainFrame() const {
  for (auto* parent : parents_) {
    if (parent->id().type == CoordinationUnitType::kFrame)
      return false;
  }
  return true;
}

}  // namespace resource_coordinator
