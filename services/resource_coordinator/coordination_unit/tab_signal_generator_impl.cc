// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/tab_signal_generator_impl.h"

#include <utility>

#include "base/values.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"

namespace resource_coordinator {

#define DISPATCH_TAB_SIGNAL(observers, METHOD, cu, ...)          \
  observers.ForAllPtrs([&](mojom::TabSignalObserver* observer) { \
    observer->METHOD(cu->id(), __VA_ARGS__);                     \
  });

TabSignalGeneratorImpl::TabSignalGeneratorImpl() = default;

TabSignalGeneratorImpl::~TabSignalGeneratorImpl() = default;

void TabSignalGeneratorImpl::AddObserver(mojom::TabSignalObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
}

bool TabSignalGeneratorImpl::ShouldObserve(
    const CoordinationUnitImpl* coordination_unit) {
  auto coordination_unit_type = coordination_unit->id().type;
  return coordination_unit_type == CoordinationUnitType::kWebContents ||
         coordination_unit_type == CoordinationUnitType::kFrame;
}

void TabSignalGeneratorImpl::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kNetworkIdle) {
    // Ignore when the signal doesn't come from main frame.
    if (!frame_cu->IsMainFrame())
      return;
    // TODO(lpy) Combine CPU usage or long task idleness signal.
    for (auto* parent : frame_cu->parents()) {
      if (parent->id().type != CoordinationUnitType::kWebContents)
        continue;
      DISPATCH_TAB_SIGNAL(observers_, OnEventReceived, parent,
                          mojom::TabEvent::kDoneLoading);
      break;
    }
  }
}

void TabSignalGeneratorImpl::OnWebContentsPropertyChanged(
    const WebContentsCoordinationUnitImpl* web_contents_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration) {
    DISPATCH_TAB_SIGNAL(observers_, OnPropertyChanged, web_contents_cu,
                        property_type, value);
  }
}

void TabSignalGeneratorImpl::BindToInterface(
    resource_coordinator::mojom::TabSignalGeneratorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace resource_coordinator
