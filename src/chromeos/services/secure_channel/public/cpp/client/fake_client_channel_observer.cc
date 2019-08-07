// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel_observer.h"

namespace chromeos {

namespace secure_channel {

FakeClientChannelObserver::FakeClientChannelObserver() = default;

FakeClientChannelObserver::~FakeClientChannelObserver() = default;

void FakeClientChannelObserver::OnDisconnected() {
  is_disconnected_ = true;
}

void FakeClientChannelObserver::OnMessageReceived(const std::string& payload) {
  received_messages_.push_back(payload);
}

}  // namespace secure_channel

}  // namespace chromeos
