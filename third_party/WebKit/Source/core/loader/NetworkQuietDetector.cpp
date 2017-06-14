// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/NetworkQuietDetector.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "platform/instrumentation/resource_coordinator/FrameResourceCoordinator.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

static const char kSupplementName[] = "NetworkQuietDetector";

NetworkQuietDetector& NetworkQuietDetector::From(Document& document) {
  NetworkQuietDetector* detector = static_cast<NetworkQuietDetector*>(
      Supplement<Document>::From(document, kSupplementName));
  if (!detector) {
    detector = new NetworkQuietDetector(document);
    Supplement<Document>::ProvideTo(document, kSupplementName, detector);
  }
  return *detector;
}

// This function is called when the number of active connections is decreased.
// Note that the number of active connections doesn't decrease monotonically.
void NetworkQuietDetector::CheckNetworkStable() {
  // Document finishes parsing after DomContentLoadedEventEnd is fired,
  // check the status in order to avoid false signals.
  if (!GetSupplementable()->HasFinishedParsing())
    return;

  SetNetworkQuietTimers(ActiveConnections());
}

NetworkQuietDetector::NetworkQuietDetector(Document& document)
    : Supplement<Document>(document),
      network_quiet_timer_(
          TaskRunnerHelper::Get(TaskType::kUnspecedTimer, &document),
          this,
          &NetworkQuietDetector::NetworkQuietTimerFired) {}

int NetworkQuietDetector::ActiveConnections() {
  ResourceFetcher* fetcher = GetSupplementable()->Fetcher();
  return fetcher->BlockingRequestCount() + fetcher->NonblockingRequestCount();
}

void NetworkQuietDetector::SetNetworkQuietTimers(int active_connections) {
  if (network_quiet_reached_ ||
      active_connections > kNetworkQuietMaximumConnections)
    return;

  // If activeConnections < 2 and the timer is already running, current
  // quiet window continues; the timer shouldn't be restarted.
  // The timer should be restarted when |active_connections| == 2 because it
  // means the number of active connections has increased to more than 2 after
  // the timer was started.
  if (active_connections == kNetworkQuietMaximumConnections ||
      !network_quiet_timer_.IsActive()) {
    network_quiet_timer_.StartOneShot(kNetworkQuietWindowSeconds,
                                      BLINK_FROM_HERE);
  }
}

void NetworkQuietDetector::NetworkQuietTimerFired(TimerBase*) {
  if (!GetSupplementable() || !GetSupplementable()->GetFrame() ||
      network_quiet_reached_ ||
      ActiveConnections() > kNetworkQuietMaximumConnections)
    return;
  network_quiet_reached_ = true;
  auto frame_resource_coordinator =
      GetSupplementable()->GetFrame()->GetFrameResourceCoordinator();
  if (frame_resource_coordinator) {
    frame_resource_coordinator->SendEvent(
        resource_coordinator::mojom::EventType::kOnLocalFrameNetworkIdle);
  }
}

DEFINE_TRACE(NetworkQuietDetector) {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
