// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"

namespace resource_coordinator {

// Frame Coordination Units form a tree structure, each FrameCoordinationUnit at
// most has one parent that is a FrameCoordinationUnit.
// A Frame Coordination Unit will have parents only if navigation committed.
class FrameCoordinationUnitImpl : public CoordinationUnitBase,
                                  public mojom::FrameCoordinationUnit {
 public:
  FrameCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~FrameCoordinationUnitImpl() override;

  // CoordinationUnitBase implementation.
  std::set<CoordinationUnitBase*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) const override;

  // FrameCoordinationUnit implementation.
  void SetAudibility(bool audible) override;
  void SetNetworkAlmostIdleness(bool idle) override;
  void OnAlertFired() override;
  void OnNonPersistentNotificationCreated() override;

  PageCoordinationUnitImpl* GetPageCoordinationUnit() const;
  bool IsMainFrame() const;

 private:
  // CoordinationUnitBase implementation.
  void OnEventReceived(const mojom::Event event) override;
  void OnPropertyChanged(const mojom::PropertyType property_type,
                         int64_t value) override;

  DISALLOW_COPY_AND_ASSIGN(FrameCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_
