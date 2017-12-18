// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/IdlenessDetector.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/probe/CoreProbes.h"
#include "platform/instrumentation/resource_coordinator/FrameResourceCoordinator.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace blink {

void IdlenessDetector::Shutdown() {
  Stop();
  local_frame_ = nullptr;
}

void IdlenessDetector::WillCommitLoad() {
  network_2_quiet_ = -1;
  network_0_quiet_ = -1;
  network_2_quiet_start_time_ = 0;
  network_0_quiet_start_time_ = 0;
}

void IdlenessDetector::DomContentLoadedEventFired() {
  if (!local_frame_)
    return;

  if (!task_observer_added_) {
    Platform::Current()->CurrentThread()->AddTaskTimeObserver(this);
    task_observer_added_ = true;
  }

  network_2_quiet_ = 0;
  network_0_quiet_ = 0;

  if (::resource_coordinator::IsPageAlmostIdleSignalEnabled()) {
    if (auto* frame_resource_coordinator =
            local_frame_->GetFrameResourceCoordinator()) {
      frame_resource_coordinator->SetNetworkAlmostIdle(false);
    }
  }
  OnDidLoadResource();
}

void IdlenessDetector::OnWillSendRequest(ResourceFetcher* fetcher) {
  // If |fetcher| is not the current fetcher of the Document, then that means
  // it's a new navigation, bail out in this case since it shouldn't affect the
  // current idleness of the local frame.
  if (!local_frame_ || fetcher != local_frame_->GetDocument()->Fetcher())
    return;

  // When OnWillSendRequest is called, the new loader hasn't been added to the
  // fetcher, thus we need to add 1 as the total request count.
  int request_count = fetcher->ActiveRequestCount() + 1;
  // If we are above the allowed number of active requests, reset timers.
  if (network_2_quiet_ >= 0 && request_count > 2)
    network_2_quiet_ = 0;
  if (network_0_quiet_ >= 0 && request_count > 0)
    network_0_quiet_ = 0;
}

// This function is called when the number of active connections is decreased.
// Note that the number of active connections doesn't decrease monotonically.
void IdlenessDetector::OnDidLoadResource() {
  if (!local_frame_)
    return;

  // Document finishes parsing after DomContentLoadedEventEnd is fired,
  // check the status in order to avoid false signals.
  if (!local_frame_->GetDocument()->HasFinishedParsing())
    return;

  // If we already reported quiet time, bail out.
  if (network_0_quiet_ < 0 && network_2_quiet_ < 0)
    return;

  int request_count =
      local_frame_->GetDocument()->Fetcher()->ActiveRequestCount();
  // If we did not achieve either 0 or 2 active connections, bail out.
  if (request_count > 2)
    return;

  double timestamp = CurrentTimeTicksInSeconds();
  // Arriving at =2 updates the quiet_2 base timestamp.
  // Arriving at <2 sets the quiet_2 base timestamp only if
  // it was not already set.
  if (request_count == 2 && network_2_quiet_ >= 0) {
    network_2_quiet_ = timestamp;
    network_2_quiet_start_time_ = timestamp;
  } else if (request_count < 2 && network_2_quiet_ == 0) {
    network_2_quiet_ = timestamp;
    network_2_quiet_start_time_ = timestamp;
  }

  if (request_count == 0 && network_0_quiet_ >= 0) {
    network_0_quiet_ = timestamp;
    network_0_quiet_start_time_ = timestamp;
  }

  if (!network_quiet_timer_.IsActive()) {
    network_quiet_timer_.StartOneShot(kNetworkQuietWatchdogSeconds, FROM_HERE);
  }
}

double IdlenessDetector::GetNetworkAlmostIdleTime() {
  return network_2_quiet_start_time_;
}

double IdlenessDetector::GetNetworkIdleTime() {
  return network_0_quiet_start_time_;
}

void IdlenessDetector::WillProcessTask(double start_time) {
  // If we have idle time and we are kNetworkQuietWindowSeconds seconds past it,
  // emit idle signals.
  DocumentLoader* loader = local_frame_->Loader().GetDocumentLoader();
  if (network_2_quiet_ > 0 &&
      start_time - network_2_quiet_ > kNetworkQuietWindowSeconds) {
    probe::lifecycleEvent(local_frame_, loader, "networkAlmostIdle",
                          network_2_quiet_start_time_);
    if (::resource_coordinator::IsPageAlmostIdleSignalEnabled()) {
      if (auto* frame_resource_coordinator =
              local_frame_->GetFrameResourceCoordinator()) {
        frame_resource_coordinator->SetNetworkAlmostIdle(true);
      }
    }
    local_frame_->GetDocument()->Fetcher()->OnNetworkQuiet();
    network_2_quiet_ = -1;
  }

  if (network_0_quiet_ > 0 &&
      start_time - network_0_quiet_ > kNetworkQuietWindowSeconds) {
    probe::lifecycleEvent(local_frame_, loader, "networkIdle",
                          network_0_quiet_start_time_);
    network_0_quiet_ = -1;
  }

  if (network_0_quiet_ < 0 && network_2_quiet_ < 0)
    Stop();
}

void IdlenessDetector::DidProcessTask(double start_time, double end_time) {
  // Shift idle timestamps with the duration of the task, we were not idle.
  if (network_2_quiet_ > 0)
    network_2_quiet_ += end_time - start_time;
  if (network_0_quiet_ > 0)
    network_0_quiet_ += end_time - start_time;
}

IdlenessDetector::IdlenessDetector(LocalFrame* local_frame)
    : local_frame_(local_frame),
      task_observer_added_(false),
      network_quiet_timer_(local_frame->GetTaskRunner(TaskType::kUnthrottled),
                           this,
                           &IdlenessDetector::NetworkQuietTimerFired) {}

void IdlenessDetector::Stop() {
  network_quiet_timer_.Stop();
  if (!task_observer_added_)
    return;
  Platform::Current()->CurrentThread()->RemoveTaskTimeObserver(this);
  task_observer_added_ = false;
}

void IdlenessDetector::NetworkQuietTimerFired(TimerBase*) {
  // TODO(lpy) Reduce the number of timers.
  if (network_0_quiet_ > 0 || network_2_quiet_ > 0) {
    network_quiet_timer_.StartOneShot(kNetworkQuietWatchdogSeconds, FROM_HERE);
  }
}

void IdlenessDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_frame_);
}

}  // namespace blink
