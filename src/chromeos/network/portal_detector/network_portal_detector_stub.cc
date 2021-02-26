// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/portal_detector/network_portal_detector_stub.h"

namespace chromeos {

NetworkPortalDetectorStub::NetworkPortalDetectorStub() = default;

NetworkPortalDetectorStub::~NetworkPortalDetectorStub() = default;

void NetworkPortalDetectorStub::AddObserver(Observer* observer) {}

void NetworkPortalDetectorStub::AddAndFireObserver(Observer* observer) {
  if (observer)
    observer->OnPortalDetectionCompleted(nullptr,
                                         CAPTIVE_PORTAL_STATUS_UNKNOWN);
}

void NetworkPortalDetectorStub::RemoveObserver(Observer* observer) {}

NetworkPortalDetector::CaptivePortalStatus
NetworkPortalDetectorStub::GetCaptivePortalStatus() {
  return CAPTIVE_PORTAL_STATUS_UNKNOWN;
}

bool NetworkPortalDetectorStub::IsEnabled() {
  return false;
}

void NetworkPortalDetectorStub::Enable(bool start_detection) {}

void NetworkPortalDetectorStub::StartPortalDetection() {}

void NetworkPortalDetectorStub::SetStrategy(
    PortalDetectorStrategy::StrategyId id) {}

}  // namespace chromeos
