// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"

#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"

namespace resource_coordinator {

FrameCoordinationUnitImpl::FrameCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitImpl(id, std::move(service_ref)) {}

FrameCoordinationUnitImpl::~FrameCoordinationUnitImpl() = default;

std::set<CoordinationUnitImpl*>
FrameCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) const {
  switch (type) {
    case CoordinationUnitType::kProcess:
    case CoordinationUnitType::kWebContents:
      // Processes and WebContents are only associated with a frame if
      // they are a direct parents of the frame.
      return GetParentCoordinationUnitsOfType(type);
    case CoordinationUnitType::kFrame: {
      // Frame Coordination Units form a tree, thus associated frame
      // coordination units are all frame coordination units in the tree. Loop
      // back to the root frame, get all child frame coordination units from the
      // root frame, add the root frame coordination unit and remove the current
      // frame coordination unit.
      const CoordinationUnitImpl* root_frame_coordination_unit = this;
      while (true) {
        bool has_parent_frame_cu = false;
        for (auto* parent : root_frame_coordination_unit->parents()) {
          if (parent->id().type == CoordinationUnitType::kFrame) {
            root_frame_coordination_unit = parent;
            has_parent_frame_cu = true;
            break;
          }
        }
        if (has_parent_frame_cu)
          continue;
        break;
      }
      auto frame_coordination_units =
          root_frame_coordination_unit->GetChildCoordinationUnitsOfType(type);
      // Insert the root frame coordination unit.
      frame_coordination_units.insert(
          const_cast<CoordinationUnitImpl*>(root_frame_coordination_unit));
      // Remove itself.
      frame_coordination_units.erase(
          const_cast<FrameCoordinationUnitImpl*>(this));
      return frame_coordination_units;
    }
    default:
      return std::set<CoordinationUnitImpl*>();
  }
}

WebContentsCoordinationUnitImpl*
FrameCoordinationUnitImpl::GetWebContentsCoordinationUnit() const {
  for (auto* parent : parents_) {
    if (parent->id().type != CoordinationUnitType::kWebContents)
      continue;
    return CoordinationUnitImpl::ToWebContentsCoordinationUnit(parent);
  }
  return nullptr;
}

bool FrameCoordinationUnitImpl::IsMainFrame() const {
  for (auto* parent : parents_) {
    if (parent->id().type == CoordinationUnitType::kFrame)
      return false;
  }
  return true;
}

void FrameCoordinationUnitImpl::OnPropertyChanged(
    const mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers())
    observer.OnFramePropertyChanged(this, property_type, value);
}

}  // namespace resource_coordinator
