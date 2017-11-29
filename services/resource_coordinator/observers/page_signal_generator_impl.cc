// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/page_signal_generator_impl.h"

#include <utility>

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace resource_coordinator {

#define DISPATCH_PAGE_SIGNAL(receivers, METHOD, ...)              \
  receivers.ForAllPtrs([&](mojom::PageSignalReceiver* receiver) { \
    receiver->METHOD(__VA_ARGS__);                                \
  });

PageSignalGeneratorImpl::PageSignalGeneratorImpl() = default;

PageSignalGeneratorImpl::~PageSignalGeneratorImpl() = default;

void PageSignalGeneratorImpl::AddReceiver(
    mojom::PageSignalReceiverPtr receiver) {
  receivers_.AddPtr(std::move(receiver));
}

bool PageSignalGeneratorImpl::ShouldObserve(
    const CoordinationUnitBase* coordination_unit) {
  auto cu_type = coordination_unit->id().type;
  return cu_type == CoordinationUnitType::kFrame ||
         cu_type == CoordinationUnitType::kProcess;
}

void PageSignalGeneratorImpl::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kNetworkAlmostIdle) {
    // Ignore if network is not idle.
    if (!value)
      return;
    NotifyPageAlmostIdleIfPossible(frame_cu);
  }
}

void PageSignalGeneratorImpl::OnProcessPropertyChanged(
    const ProcessCoordinationUnitImpl* process_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration) {
    for (auto* frame_cu : process_cu->GetFrameCoordinationUnits()) {
      if (!frame_cu->IsMainFrame())
        continue;
      auto* page_cu = frame_cu->GetPageCoordinationUnit();
      int64_t duration;
      if (!page_cu || !page_cu->GetExpectedTaskQueueingDuration(&duration))
        continue;
      DISPATCH_PAGE_SIGNAL(receivers_, SetExpectedTaskQueueingDuration,
                           page_cu->id(),
                           base::TimeDelta::FromMilliseconds(duration));
    }
  } else if (property_type == mojom::PropertyType::kMainThreadTaskLoadIsLow) {
    for (auto* frame_cu : process_cu->GetFrameCoordinationUnits())
      NotifyPageAlmostIdleIfPossible(frame_cu);
  }
}

void PageSignalGeneratorImpl::BindToInterface(
    resource_coordinator::mojom::PageSignalGeneratorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

void PageSignalGeneratorImpl::NotifyPageAlmostIdleIfPossible(
    const FrameCoordinationUnitImpl* frame_cu) {
  DCHECK(IsPageAlmostIdleSignalEnabled());
  if (!frame_cu->IsMainFrame())
    return;

  auto* page_cu = frame_cu->GetPageCoordinationUnit();
  if (!page_cu)
    return;

  // If the page is already in almost idle state, don't deliver the signal.
  // PageAlmostIdle signal shouldn't be delivered again if the frame is already
  // in almost idle state.
  if (page_cu->WasAlmostIdle())
    return;
  if (!page_cu->CheckAndUpdateAlmostIdleStateIfNeeded())
    return;
  DISPATCH_PAGE_SIGNAL(receivers_, NotifyPageAlmostIdle, page_cu->id());
}

}  // namespace resource_coordinator
