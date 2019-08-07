// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_message_receiver.h"

namespace chromeos {

namespace secure_channel {

FakeMessageReceiver::FakeMessageReceiver() = default;

FakeMessageReceiver::~FakeMessageReceiver() = default;

void FakeMessageReceiver::OnMessageReceived(const std::string& message) {
  received_messages_.push_back(message);
}

}  // namespace secure_channel

}  // namespace chromeos
