// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_CONNECTION_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_CONNECTION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/secure_channel/connection.h"
#include "chromeos/services/secure_channel/secure_channel.h"

namespace chromeos {

namespace secure_channel {

// A fake implementation of SecureChannel to use in tests.
class FakeSecureChannelConnection : public SecureChannel {
 public:
  FakeSecureChannelConnection(std::unique_ptr<Connection> connection);
  ~FakeSecureChannelConnection() override;

  void set_destructor_callback(base::OnceClosure destructor_callback) {
    destructor_callback_ = std::move(destructor_callback);
  }

  bool was_initialized() { return was_initialized_; }

  void set_rssi_to_return(const base::Optional<int32_t>& rssi_to_return) {
    rssi_to_return_ = rssi_to_return;
  }

  void set_channel_binding_data(
      const base::Optional<std::string>& channel_binding_data) {
    channel_binding_data_ = channel_binding_data;
  }

  struct SentMessage {
    SentMessage(const std::string& feature, const std::string& payload);

    std::string feature;
    std::string payload;
  };

  void ChangeStatus(const Status& new_status);
  void ReceiveMessage(const std::string& feature, const std::string& payload);
  void CompleteSendingMessage(int sequence_number);

  std::vector<Observer*> observers() { return observers_; }

  std::vector<SentMessage> sent_messages() { return sent_messages_; }

  // SecureChannel:
  void Initialize() override;
  int SendMessage(const std::string& feature,
                  const std::string& payload) override;
  void Disconnect() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetConnectionRssi(
      base::OnceCallback<void(base::Optional<int32_t>)> callback) override;
  base::Optional<std::string> GetChannelBindingData() override;

 private:
  int next_sequence_number_ = 0;
  bool was_initialized_ = false;
  std::vector<Observer*> observers_;
  std::vector<SentMessage> sent_messages_;
  base::Optional<int32_t> rssi_to_return_;
  base::Optional<std::string> channel_binding_data_;

  base::OnceClosure destructor_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureChannelConnection);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_CONNECTION_H_
