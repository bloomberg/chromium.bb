// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/remote_host_info.h"

#include "base/logging.h"

namespace remoting {
namespace test {

RemoteHostInfo::RemoteHostInfo() :
    remote_host_status(kRemoteHostStatusUnknown) {}

RemoteHostInfo::~RemoteHostInfo() {}

bool RemoteHostInfo::IsReadyForConnection() const {
  return (remote_host_status == kRemoteHostStatusReady);
}

void RemoteHostInfo::SetRemoteHostStatusFromString(
    const std::string& status_string) {
  if (status_string == "done") {
    remote_host_status = kRemoteHostStatusReady;
  } else if (status_string == "pending") {
    remote_host_status = kRemoteHostStatusPending;
  } else {
    LOG(WARNING) << "Unknown status passed in: " << status_string;
    remote_host_status = kRemoteHostStatusUnknown;
  }
}

}  // namespace test
}  // namespace remoting
