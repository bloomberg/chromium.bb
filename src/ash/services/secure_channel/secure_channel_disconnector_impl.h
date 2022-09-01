// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_IMPL_H_
#define ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_IMPL_H_

#include <memory>

#include "ash/services/secure_channel/secure_channel.h"
#include "ash/services/secure_channel/secure_channel_disconnector.h"
#include "base/containers/flat_set.h"

namespace ash::secure_channel {

// Concrete SecureChannelDisconnector implementation.
class SecureChannelDisconnectorImpl : public SecureChannelDisconnector,
                                      public SecureChannel::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<SecureChannelDisconnector> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SecureChannelDisconnector> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  SecureChannelDisconnectorImpl(const SecureChannelDisconnectorImpl&) = delete;
  SecureChannelDisconnectorImpl& operator=(
      const SecureChannelDisconnectorImpl&) = delete;

  ~SecureChannelDisconnectorImpl() override;

 private:
  SecureChannelDisconnectorImpl();

  // SecureChannelDisconnector:
  void DisconnectSecureChannel(
      std::unique_ptr<SecureChannel> channel_to_disconnect) override;

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override;

  base::flat_set<std::unique_ptr<SecureChannel>> disconnecting_channels_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_IMPL_H_
