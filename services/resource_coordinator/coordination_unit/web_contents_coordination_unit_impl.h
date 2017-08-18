// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_WEB_CONTENTS_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_WEB_CONTENTS_COORDINATION_UNIT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"

namespace resource_coordinator {

class WebContentsCoordinationUnitImpl : public CoordinationUnitImpl {
 public:
  WebContentsCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~WebContentsCoordinationUnitImpl() override;

  // CoordinationUnitImpl implementation.
  std::set<CoordinationUnitImpl*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) const override;
  void RecalculateProperty(const mojom::PropertyType property_type) override;

  bool IsVisible() const;

 private:
  // CoordinationUnitImpl implementation.
  void OnEventReceived(const mojom::Event event) override;
  void OnPropertyChanged(const mojom::PropertyType property_type,
                         int64_t value) override;
  double CalculateCPUUsage();

  // Returns true for a valid value. Returns false otherwise.
  bool CalculateExpectedTaskQueueingDuration(int64_t* output);

  // Returns the main frame CU or nullptr if this tab has no main frame.
  CoordinationUnitImpl* GetMainFrameCoordinationUnit();

  DISALLOW_COPY_AND_ASSIGN(WebContentsCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_WEB_CONTENTS_COORDINATION_UNIT_IMPL_H_
