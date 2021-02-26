// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_TESTING_SIMPLE_MESSAGE_PORT_H_
#define CAST_STREAMING_TESTING_SIMPLE_MESSAGE_PORT_H_

#include <string>
#include <utility>
#include <vector>

#include "cast/common/public/message_port.h"
#include "cast/streaming/message_fields.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {

class SimpleMessagePort : public MessagePort {
 public:
  ~SimpleMessagePort() override {}
  void SetClient(MessagePort::Client* client,
                 std::string client_sender_id) override {
    client_ = client;
  }

  void ResetClient() override { client_ = nullptr; }

  void ReceiveMessage(const std::string& message) {
    ReceiveMessage(kCastWebrtcNamespace, message);
  }

  void ReceiveMessage(const std::string& namespace_,
                      const std::string message) {
    ASSERT_NE(client_, nullptr);
    client_->OnMessage("sender-1234", namespace_, message);
  }

  void ReceiveError(Error error) {
    ASSERT_NE(client_, nullptr);
    client_->OnError(error);
  }

  void PostMessage(const std::string& sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override {
    posted_messages_.emplace_back(message);
  }

  MessagePort::Client* client() const { return client_; }
  const std::vector<std::string> posted_messages() const {
    return posted_messages_;
  }

 private:
  MessagePort::Client* client_ = nullptr;
  std::vector<std::string> posted_messages_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_TESTING_SIMPLE_MESSAGE_PORT_H_
