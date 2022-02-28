// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace chromeos {
namespace secure_channel {

class FakeConnectionManager : public secure_channel::ConnectionManager {
 public:
  FakeConnectionManager();
  ~FakeConnectionManager() override;

  using ConnectionManager::NotifyMessageReceived;

  void SetStatus(Status status);
  const std::vector<std::string>& sent_messages() const {
    return sent_messages_;
  }

  void set_register_payload_file_result(bool result) {
    register_payload_file_result_ = result;
  }

  void SendFileTransferUpdate(mojom::FileTransferUpdatePtr update);

  size_t num_attempt_connection_calls() const {
    return num_attempt_connection_calls_;
  }

  size_t num_disconnect_calls() const { return num_disconnect_calls_; }

 private:
  // ConnectionManager:
  Status GetStatus() const override;
  void AttemptNearbyConnection() override;
  void Disconnect() override;
  void SendMessage(const std::string& payload) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;

  Status status_;
  std::vector<std::string> sent_messages_;
  bool register_payload_file_result_ = true;
  base::flat_map<int64_t,
                 base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>>
      file_transfer_update_callbacks_;
  size_t num_attempt_connection_calls_ = 0;
  size_t num_disconnect_calls_ = 0;
};

}  // namespace secure_channel
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash {
namespace secure_channel {
using ::chromeos::secure_channel::FakeConnectionManager;
}  // namespace secure_channel
}  // namespace ash

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_
