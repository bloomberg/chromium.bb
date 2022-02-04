// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace chromeos {

namespace secure_channel {

// Test double implementation of ClientChannel.
class FakeClientChannel : public ClientChannel {
 public:
  FakeClientChannel();

  FakeClientChannel(const FakeClientChannel&) = delete;
  FakeClientChannel& operator=(const FakeClientChannel&) = delete;

  ~FakeClientChannel() override;

  using ClientChannel::NotifyDisconnected;
  using ClientChannel::NotifyMessageReceived;

  void InvokePendingGetConnectionMetadataCallback(
      mojom::ConnectionMetadataPtr connection_metadata);

  std::vector<std::pair<std::string, base::OnceClosure>>& sent_messages() {
    return sent_messages_;
  }

  const std::vector<int64_t>& registered_file_payloads() const {
    return registered_file_payloads_;
  }

  void set_destructor_callback(base::OnceClosure callback) {
    destructor_callback_ = std::move(callback);
  }

 private:
  friend class SecureChannelClientChannelImplTest;

  // ClientChannel:
  void PerformGetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& payload,
                          base::OnceClosure on_sent_callback) override;
  void PerformRegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;

  // Queues up callbacks passed into PerformGetConnectionMetadata(), to be
  // invoked later.
  std::queue<base::OnceCallback<void(mojom::ConnectionMetadataPtr)>>
      get_connection_metadata_callback_queue_;
  std::vector<std::pair<std::string, base::OnceClosure>> sent_messages_;
  std::vector<int64_t> registered_file_payloads_;
  base::OnceClosure destructor_callback_;
};

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when this file is moved to ash.
namespace ash {
namespace secure_channel {
using ::chromeos::secure_channel::FakeClientChannel;
}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_H_
