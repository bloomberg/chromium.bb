// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/page_signal_generator_impl.h"

#include <utility>

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace resource_coordinator {

#define DISPATCH_PAGE_SIGNAL(receivers, METHOD, ...)              \
  receivers.ForAllPtrs([&](mojom::PageSignalReceiver* receiver) { \
    receiver->METHOD(__VA_ARGS__);                                \
  });

// static
constexpr base::TimeDelta PageSignalGeneratorImpl::kLoadedAndIdlingTimeout =
    base::TimeDelta::FromSeconds(1);

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
         cu_type == CoordinationUnitType::kPage ||
         cu_type == CoordinationUnitType::kProcess;
}

void PageSignalGeneratorImpl::OnCoordinationUnitCreated(
    const CoordinationUnitBase* cu) {
  auto cu_type = cu->id().type;
  if (cu_type != CoordinationUnitType::kPage)
    return;

  // Create page data exists for this Page CU.
  auto* page_cu = static_cast<const PageCoordinationUnitImpl*>(cu);
  DCHECK(!base::ContainsKey(page_data_, page_cu)); // No data should exist yet.
  page_data_[page_cu].load_idle_state = kLoadingNotStarted;
}

void PageSignalGeneratorImpl::OnBeforeCoordinationUnitDestroyed(
    const CoordinationUnitBase* cu) {
  auto cu_type = cu->id().type;
  if (cu_type != CoordinationUnitType::kPage)
    return;
  auto* page_cu = static_cast<const PageCoordinationUnitImpl*>(cu);
  size_t count = page_data_.erase(page_cu);
  DCHECK_EQ(1u, count);  // This should always erase exactly one CU.
}

void PageSignalGeneratorImpl::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  // Only the network idle state of a frame is of interest.
  if (property_type != mojom::PropertyType::kNetworkAlmostIdle)
    return;
  UpdateLoadIdleStateFrame(frame_cu);
}

void PageSignalGeneratorImpl::OnPagePropertyChanged(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  // Only the loading state of a page is of interest.
  if (property_type != mojom::PropertyType::kIsLoading)
    return;
  UpdateLoadIdleStatePage(page_cu);
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
    UpdateLoadIdleStateProcess(process_cu);
  }
}

void PageSignalGeneratorImpl::OnPageEventReceived(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::Event event) {
  // Only the navigation committed event is of interest.
  if (event != mojom::Event::kNavigationCommitted)
    return;

  // Reset the load-idle state associated with this page as a new navigation has
  // started.
  auto* page_data = GetPageData(page_cu);
  page_data->load_idle_state = kLoadingNotStarted;
  page_data->idling_timer.Stop();
}

void PageSignalGeneratorImpl::BindToInterface(
    resource_coordinator::mojom::PageSignalGeneratorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

void PageSignalGeneratorImpl::UpdateLoadIdleStateFrame(
    const FrameCoordinationUnitImpl* frame_cu) {
  // Only main frames are relevant in the load idle state.
  if (!frame_cu->IsMainFrame())
    return;

  // Update the load idle state of the page associated with this frame.
  auto* page_cu = frame_cu->GetPageCoordinationUnit();
  if (!page_cu)
    return;
  UpdateLoadIdleStatePage(page_cu);
}

void PageSignalGeneratorImpl::UpdateLoadIdleStatePage(
    const PageCoordinationUnitImpl* page_cu) {
  auto* page_data = GetPageData(page_cu);

  // Once the cycle is complete state transitions are no longer tracked for this
  // page.
  if (page_data->load_idle_state == kLoadedAndIdle)
    return;

  // Cancel any ongoing timers. A new timer will be set if necessary.
  page_data->idling_timer.Stop();
  base::TimeTicks now = ResourceCoordinatorClock::NowTicks();

  switch (page_data->load_idle_state) {
    case kLoadingNotStarted: {
      if (!IsLoading(page_cu))
        return;
      page_data->load_idle_state = kLoading;
      return;
    }

    case kLoading: {
      if (IsLoading(page_cu))
        return;
      page_data->load_idle_state = kLoadedNotIdling;
      // Let the kLoadedNotIdling state transition evaluate, allowing an
      // effective transition directly from kLoading to kLoadedAndIdling.
      FALLTHROUGH;
    }

    case kLoadedNotIdling: {
      if (!IsIdling(page_cu))
        return;
      page_data->load_idle_state = kLoadedAndIdling;
      page_data->idling_started = now;
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    case kLoadedAndIdling: {
      // If the page is not still idling then transition back a state. The timer
      // has already been canceled above so future calls will only be due to
      // potential changes in idling state.
      if (!IsIdling(page_cu)) {
        page_data->load_idle_state = kLoadedNotIdling;
        return;
      }

      // Idling has been occurred long enough then make the last state
      // transition.
      if (now - page_data->idling_started >= kLoadedAndIdlingTimeout) {
        page_data->load_idle_state = kLoadedAndIdle;
        // Notify observers that the page is loaded and idle.
        DISPATCH_PAGE_SIGNAL(receivers_, NotifyPageAlmostIdle, page_cu->id());
        return;
      }

      break;
    }

    // This should never occur.
    case kLoadedAndIdle:
      NOTREACHED();
  }

  // Getting here means a new timer needs to be set.
  DCHECK_EQ(kLoadedAndIdling, page_data->load_idle_state);
  base::TimeDelta idling_timeout =
      (page_data->idling_started + kLoadedAndIdlingTimeout) - now;
  page_data->idling_timer.Start(
      FROM_HERE, idling_timeout,
      base::Bind(&PageSignalGeneratorImpl::UpdateLoadIdleStatePage,
                 base::Unretained(this), page_cu));
}

void PageSignalGeneratorImpl::UpdateLoadIdleStateProcess(
    const ProcessCoordinationUnitImpl* process_cu) {
  for (auto* frame_cu : process_cu->GetFrameCoordinationUnits())
    UpdateLoadIdleStateFrame(frame_cu);
}

PageSignalGeneratorImpl::PageData* PageSignalGeneratorImpl::GetPageData(
    const PageCoordinationUnitImpl* page_cu) {
  // There are two ways to enter this function:
  // 1. Via On*PropertyChange calls. The backing PageData is guaranteed to
  //    exist in this case as the lifetimes are managed by the CU graph.
  // 2. Via a timer stored in a PageData. The backing PageData will be
  //    guaranteed to exist in this case as well, as otherwise the timer will
  //    have been canceled.
  DCHECK(base::ContainsKey(page_data_, page_cu));
  return &page_data_[page_cu];
}

bool PageSignalGeneratorImpl::IsLoading(
    const PageCoordinationUnitImpl* page_cu) {
  int64_t is_loading = 0;
  if (!page_cu->GetProperty(mojom::PropertyType::kIsLoading, &is_loading))
    return false;
  return is_loading;
}

bool PageSignalGeneratorImpl::IsIdling(
    const PageCoordinationUnitImpl* page_cu) {
  // Get the Frame CU for the main frame associated with this page.
  const FrameCoordinationUnitImpl* main_frame_cu =
      page_cu->GetMainFrameCoordinationUnit();
  if (!main_frame_cu)
    return false;

  // Get the process CU associated with this main frame.
  const auto* process_cu = main_frame_cu->GetProcessCoordinationUnit();
  if (!process_cu)
    return false;

  // Note that it's possible for one misbehaving frame hosted in the same
  // process as this page's main frame to keep the main thread task low high.
  // In this case the IsIdling signal will be delayed, despite the task load
  // associated with this page's main frame actually being low. In the case
  // of session restore this is mitigated by having a timeout while waiting for
  // this signal.
  return
      main_frame_cu->GetPropertyOrDefault(
          mojom::PropertyType::kNetworkAlmostIdle, 0u) &&
      process_cu->GetPropertyOrDefault(
          mojom::PropertyType::kMainThreadTaskLoadIsLow, 0u);
}

}  // namespace resource_coordinator
