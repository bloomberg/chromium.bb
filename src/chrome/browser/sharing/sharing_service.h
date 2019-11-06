// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "components/keyed_service/core/keyed_service.h"

// Class to manage lifecycle of sharing feature, and provide APIs to send
// sharing messages to other devices.
class SharingService : public KeyedService {
 public:
  SharingService();
  ~SharingService() override;

  // Returns a list of DeviceInfo that is available to receive messages.
  // All returned devices has the specified |required_capabilities| defined in
  // SharingDeviceCapability enum.
  std::vector<SharingDeviceInfo> GetDeviceCandidates(
      int required_capabilities) const;

  // Sends a message to the device specified by GUID.
  bool SendMessage(const std::string& device_guid,
                   const chrome_browser_sharing::SharingMessage& message);

  // Registers a handler of a given SharingMessage payload type.
  void RegisterHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_type,
      SharingMessageHandler* handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(SharingService);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_H_
