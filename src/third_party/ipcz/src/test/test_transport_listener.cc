// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/test_transport_listener.h"

#include <utility>

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz::test {

TestTransportListener::TestTransportListener(IpczHandle node,
                                             IpczDriverHandle handle)
    : TestTransportListener(MakeRefCounted<DriverTransport>(
          DriverObject(WrapRefCounted(reinterpret_cast<Node*>(node)),
                       handle))) {}

TestTransportListener::TestTransportListener(Ref<DriverTransport> transport)
    : transport_(std::move(transport)) {
  transport_->set_listener(this);
}

TestTransportListener::~TestTransportListener() {
  StopListening();
}

void TestTransportListener::StopListening() {
  if (!activated_) {
    return;
  }

  transport_->Deactivate();
  activated_ = false;
}

void TestTransportListener::OnRawMessage(GenericMessageHandler handler) {
  ABSL_ASSERT(!message_handler_);
  message_handler_ = std::move(handler);
  ActivateTransportIfNecessary();
}

void TestTransportListener::OnStringMessage(
    std::function<void(std::string_view)> handler) {
  OnRawMessage([handler](const DriverTransport::Message& message) {
    EXPECT_TRUE(message.handles.empty());
    handler(std::string_view(reinterpret_cast<const char*>(message.data.data()),
                             message.data.size()));
    return IPCZ_RESULT_OK;
  });
}

void TestTransportListener::OnError(ErrorHandler handler) {
  ABSL_ASSERT(!error_handler_);
  error_handler_ = std::move(handler);
  ActivateTransportIfNecessary();
}

void TestTransportListener::ActivateTransportIfNecessary() {
  if (activated_) {
    return;
  }

  activated_ = true;
  transport_->Activate();
}

IpczResult TestTransportListener::OnTransportMessage(
    const DriverTransport::Message& message) {
  ABSL_ASSERT(message_handler_);
  return message_handler_(message);
}

void TestTransportListener::OnTransportError() {
  if (error_handler_) {
    error_handler_();
  }
};

}  // namespace ipcz::test
