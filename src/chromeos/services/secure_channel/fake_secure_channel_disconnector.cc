// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_secure_channel_disconnector.h"

namespace chromeos {

namespace secure_channel {

FakeSecureChannelDisconnector::FakeSecureChannelDisconnector() = default;

FakeSecureChannelDisconnector::~FakeSecureChannelDisconnector() = default;

bool FakeSecureChannelDisconnector::WasChannelHandled(
    SecureChannel* secure_channel) {
  for (const auto& channel : handled_channels_) {
    if (channel.get() == secure_channel)
      return true;
  }
  return false;
}

void FakeSecureChannelDisconnector::DisconnectSecureChannel(
    std::unique_ptr<SecureChannel> channel_to_disconnect) {
  handled_channels_.insert(std::move(channel_to_disconnect));
}

}  // namespace secure_channel

}  // namespace chromeos
