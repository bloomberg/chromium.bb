// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

SharingService::SharingService() = default;

SharingService::~SharingService() = default;

std::vector<SharingDeviceInfo> SharingService::GetDeviceCandidates(
    int required_capabilities) const {
  return std::vector<SharingDeviceInfo>();
}

bool SharingService::SendMessage(
    const std::string& device_guid,
    const chrome_browser_sharing::SharingMessage& message) {
  return true;
}

void SharingService::RegisterHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_type,
    SharingMessageHandler* handler) {}
