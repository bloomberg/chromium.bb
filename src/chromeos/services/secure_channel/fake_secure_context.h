// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CONTEXT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CONTEXT_H_

#include <string>

#include "base/callback.h"
#include "chromeos/services/secure_channel/secure_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace secure_channel {

class FakeSecureContext : public SecureContext {
 public:
  FakeSecureContext();

  FakeSecureContext(const FakeSecureContext&) = delete;
  FakeSecureContext& operator=(const FakeSecureContext&) = delete;

  ~FakeSecureContext() override;

  // SecureContext:
  ProtocolVersion GetProtocolVersion() const override;
  std::string GetChannelBindingData() const override;
  void Encode(const std::string& message,
              EncodeMessageCallback callback) override;
  void DecodeAndDequeue(const std::string& encoded_message,
                        DecodeMessageCallback callback) override;

  void set_protocol_version(ProtocolVersion protocol_version) {
    protocol_version_ = protocol_version;
  }

  void set_channel_binding_data(const std::string channel_binding_data) {
    channel_binding_data_ = channel_binding_data;
  }

 private:
  ProtocolVersion protocol_version_;
  absl::optional<std::string> channel_binding_data_;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CONTEXT_H_
