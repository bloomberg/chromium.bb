// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_DISCONNECTOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_DISCONNECTOR_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/secure_channel.h"
#include "chromeos/services/secure_channel/secure_channel_disconnector.h"

namespace chromeos {

namespace secure_channel {

// Test SecureChannelDisconnector implementation.
class FakeSecureChannelDisconnector : public SecureChannelDisconnector {
 public:
  FakeSecureChannelDisconnector();
  ~FakeSecureChannelDisconnector() override;

  const base::flat_set<std::unique_ptr<SecureChannel>>& handled_channels() {
    return handled_channels_;
  }

  bool WasChannelHandled(SecureChannel* secure_channel);

 private:
  // SecureChannelDisconnector:
  void DisconnectSecureChannel(
      std::unique_ptr<SecureChannel> channel_to_disconnect) override;

  base::flat_set<std::unique_ptr<SecureChannel>> handled_channels_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureChannelDisconnector);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_DISCONNECTOR_H_
