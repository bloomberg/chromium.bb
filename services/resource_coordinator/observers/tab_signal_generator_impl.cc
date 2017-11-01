// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/tab_signal_generator_impl.h"

#include <utility>

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace resource_coordinator {

#define DISPATCH_TAB_SIGNAL(observers, METHOD, ...)              \
  observers.ForAllPtrs([&](mojom::TabSignalObserver* observer) { \
    observer->METHOD(__VA_ARGS__);                               \
  });

TabSignalGeneratorImpl::TabSignalGeneratorImpl() = default;

TabSignalGeneratorImpl::~TabSignalGeneratorImpl() = default;

void TabSignalGeneratorImpl::AddObserver(mojom::TabSignalObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
}

bool TabSignalGeneratorImpl::ShouldObserve(
    const CoordinationUnitBase* coordination_unit) {
  auto cu_type = coordination_unit->id().type;
  return cu_type == CoordinationUnitType::kFrame ||
         cu_type == CoordinationUnitType::kProcess;
}

void TabSignalGeneratorImpl::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kNetworkAlmostIdle) {
    // Ignore when the signal doesn't come from main frame.
    if (!frame_cu->IsMainFrame())
      return;
    // TODO(lpy) Combine CPU usage or long task idleness signal.
    if (auto* page_cu = frame_cu->GetPageCoordinationUnit()) {
      DISPATCH_TAB_SIGNAL(observers_, NotifyPageAlmostIdle, page_cu->id());
    }
  }
}

void TabSignalGeneratorImpl::OnProcessPropertyChanged(
    const ProcessCoordinationUnitImpl* process_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration) {
    for (auto* frame_cu : process_cu->GetFrameCoordinationUnits()) {
      if (!frame_cu->IsMainFrame())
        continue;
      auto* page_cu = frame_cu->GetPageCoordinationUnit();
      int64_t duration;
      if (!page_cu->GetExpectedTaskQueueingDuration(&duration))
        continue;
      DISPATCH_TAB_SIGNAL(observers_, SetExpectedTaskQueueingDuration,
                          page_cu->id(),
                          base::TimeDelta::FromMilliseconds(duration));
    }
  }
}

void TabSignalGeneratorImpl::BindToInterface(
    resource_coordinator::mojom::TabSignalGeneratorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace resource_coordinator
