// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/page_signal_generator_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace performance_manager {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BloatedRendererHandlingInResourceCoordinator {
  kForwardedToBrowser = 0,
  kIgnoredDueToMultiplePages = 1,
  kMaxValue = kIgnoredDueToMultiplePages
};

void RecordBloatedRendererHandling(
    BloatedRendererHandlingInResourceCoordinator handling) {
  UMA_HISTOGRAM_ENUMERATION("BloatedRenderer.HandlingInResourceCoordinator",
                            handling);
}

}  // anonymous namespace

PageSignalGeneratorImpl::PageSignalGeneratorImpl() = default;

PageSignalGeneratorImpl::~PageSignalGeneratorImpl() = default;

void PageSignalGeneratorImpl::AddReceiver(
    resource_coordinator::mojom::PageSignalReceiverPtr receiver) {
  receivers_.AddPtr(std::move(receiver));
}

bool PageSignalGeneratorImpl::ShouldObserve(const NodeBase* coordination_unit) {
  auto cu_type = coordination_unit->id().type;
  switch (cu_type) {
    case resource_coordinator::CoordinationUnitType::kFrame:
    case resource_coordinator::CoordinationUnitType::kPage:
    case resource_coordinator::CoordinationUnitType::kProcess:
    case resource_coordinator::CoordinationUnitType::kSystem:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

void PageSignalGeneratorImpl::OnNodeCreated(NodeBase* cu) {
  auto cu_type = cu->id().type;
  if (cu_type != resource_coordinator::CoordinationUnitType::kPage)
    return;

  // Create page data exists for this Page CU.
  auto* page_node = PageNodeImpl::FromNodeBase(cu);
  DCHECK(
      !base::ContainsKey(page_data_, page_node));  // No data should exist yet.
  page_data_[page_node].Reset();
}

void PageSignalGeneratorImpl::OnBeforeNodeDestroyed(NodeBase* cu) {
  auto cu_type = cu->id().type;
  if (cu_type != resource_coordinator::CoordinationUnitType::kPage)
    return;

  auto* page_node = PageNodeImpl::FromNodeBase(cu);
  size_t count = page_data_.erase(page_node);
  DCHECK_EQ(1u, count);  // This should always erase exactly one CU.
}

void PageSignalGeneratorImpl::OnPagePropertyChanged(
    PageNodeImpl* page_node,
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  if (property_type ==
      resource_coordinator::mojom::PropertyType::kLifecycleState) {
    DispatchPageSignal(
        page_node,
        &resource_coordinator::mojom::PageSignalReceiver::SetLifecycleState,
        static_cast<resource_coordinator::mojom::LifecycleState>(value));
  }
}

void PageSignalGeneratorImpl::OnProcessPropertyChanged(
    ProcessNodeImpl* process_node,
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == resource_coordinator::mojom::PropertyType::
                           kExpectedTaskQueueingDuration) {
    for (auto* frame_node : process_node->GetFrameNodes()) {
      if (!frame_node->IsMainFrame())
        continue;
      auto* page_node = frame_node->GetPageNode();
      int64_t duration;
      if (!page_node || !page_node->GetExpectedTaskQueueingDuration(&duration))
        continue;
      DispatchPageSignal(page_node,
                         &resource_coordinator::mojom::PageSignalReceiver::
                             SetExpectedTaskQueueingDuration,
                         base::TimeDelta::FromMilliseconds(duration));
    }
  }
}

void PageSignalGeneratorImpl::OnFrameEventReceived(
    FrameNodeImpl* frame_node,
    resource_coordinator::mojom::Event event) {
  if (event !=
      resource_coordinator::mojom::Event::kNonPersistentNotificationCreated)
    return;

  auto* page_node = frame_node->GetPageNode();
  if (!page_node)
    return;

  DispatchPageSignal(page_node,
                     &resource_coordinator::mojom::PageSignalReceiver::
                         NotifyNonPersistentNotificationCreated);
}

void PageSignalGeneratorImpl::OnProcessEventReceived(
    ProcessNodeImpl* process_node,
    resource_coordinator::mojom::Event event) {
  if (event == resource_coordinator::mojom::Event::kRendererIsBloated) {
    std::set<PageNodeImpl*> page_nodes =
        process_node->GetAssociatedPageCoordinationUnits();
    // Currently bloated renderer handling supports only a single page.
    if (page_nodes.size() == 1u) {
      auto* page_node = *page_nodes.begin();
      DispatchPageSignal(page_node,
                         &resource_coordinator::mojom::PageSignalReceiver::
                             NotifyRendererIsBloated);
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInResourceCoordinator::kForwardedToBrowser);
    } else {
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInResourceCoordinator::
              kIgnoredDueToMultiplePages);
    }
  }
}

void PageSignalGeneratorImpl::OnSystemEventReceived(
    SystemNodeImpl* system_node,
    resource_coordinator::mojom::Event event) {
  if (event == resource_coordinator::mojom::Event::kProcessCPUUsageReady) {
    base::TimeTicks measurement_start =
        system_node->last_measurement_start_time();

    for (auto& entry : page_data_) {
      const PageNodeImpl* page = entry.first;
      PageData* data = &entry.second;
      // TODO(siggi): Figure "recency" here, to avoid firing a measurement event
      //     for state transitions that happened "too long" before a
      //     measurement started. Alternatively perhaps this bit of policy is
      //     better done in the observer, in which case it needs the time stamps
      //     involved.
      if (page->page_almost_idle() && !data->performance_estimate_issued &&
          data->last_state_change < measurement_start) {
        DispatchPageSignal(page,
                           &resource_coordinator::mojom::PageSignalReceiver::
                               OnLoadTimePerformanceEstimate,
                           page->TimeSinceLastNavigation(),
                           page->cumulative_cpu_usage_estimate(),
                           page->private_footprint_kb_estimate());
        data->performance_estimate_issued = true;
      }
    }
  }
}

void PageSignalGeneratorImpl::OnPageAlmostIdleChanged(PageNodeImpl* page_node) {
  auto* data = GetPageData(page_node);
  data->Reset();

  if (page_node->page_almost_idle()) {
    // Notify observers that the page is loaded and idle.
    DispatchPageSignal(
        page_node,
        &resource_coordinator::mojom::PageSignalReceiver::NotifyPageAlmostIdle);
  }
}

void PageSignalGeneratorImpl::BindToInterface(
    resource_coordinator::mojom::PageSignalGeneratorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

PageSignalGeneratorImpl::PageData* PageSignalGeneratorImpl::GetPageData(
    const PageNodeImpl* page_node) {
  DCHECK(resource_coordinator::IsPageAlmostIdleSignalEnabled());
  // There are two ways to enter this function:
  // 1. Via On*PropertyChange calls. The backing PageData is guaranteed to
  //    exist in this case as the lifetimes are managed by the CU graph.
  // 2. Via a timer stored in a PageData. The backing PageData will be
  //    guaranteed to exist in this case as well, as otherwise the timer will
  //    have been canceled.
  DCHECK(base::ContainsKey(page_data_, page_node));
  return &page_data_[page_node];
}

template <typename Method, typename... Params>
void PageSignalGeneratorImpl::DispatchPageSignal(const PageNodeImpl* page_node,
                                                 Method m,
                                                 Params... params) {
  receivers_.ForAllPtrs(
      [&](resource_coordinator::mojom::PageSignalReceiver* receiver) {
        (receiver->*m)(
            resource_coordinator::PageNavigationIdentity{
                page_node->id(), page_node->navigation_id(),
                page_node->main_frame_url()},
            std::forward<Params>(params)...);
      });
}

void PageSignalGeneratorImpl::PageData::Reset() {
  last_state_change = ResourceCoordinatorClock::NowTicks();
  performance_estimate_issued = false;
}

}  // namespace performance_manager
