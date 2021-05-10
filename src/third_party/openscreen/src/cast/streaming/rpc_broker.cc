// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/rpc_broker.h"

#include <utility>

#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

namespace {

std::ostream& operator<<(std::ostream& out, const RpcMessage& message) {
  out << "handle=" << message.handle() << ", proc=" << message.proc();
  switch (message.rpc_oneof_case()) {
    case RpcMessage::kIntegerValue:
      return out << ", integer_value=" << message.integer_value();
    case RpcMessage::kInteger64Value:
      return out << ", integer64_value=" << message.integer64_value();
    case RpcMessage::kDoubleValue:
      return out << ", double_value=" << message.double_value();
    case RpcMessage::kBooleanValue:
      return out << ", boolean_value=" << message.boolean_value();
    case RpcMessage::kStringValue:
      return out << ", string_value=" << message.string_value();
    default:
      return out << ", rpc_oneof=" << message.rpc_oneof_case();
  }

  OSP_NOTREACHED();
}

}  // namespace

RpcBroker::RpcBroker(SendMessageCallback send_message_cb)
    : next_handle_(kFirstHandle),
      send_message_cb_(std::move(send_message_cb)) {}

RpcBroker::~RpcBroker() {
  receive_callbacks_.clear();
}

RpcBroker::Handle RpcBroker::GetUniqueHandle() {
  return next_handle_++;
}

void RpcBroker::RegisterMessageReceiverCallback(
    RpcBroker::Handle handle,
    ReceiveMessageCallback callback) {
  OSP_DCHECK(receive_callbacks_.find(handle) == receive_callbacks_.end())
      << "must deregister before re-registering";
  OSP_DVLOG << "registering handle: " << handle;
  receive_callbacks_.emplace_back(handle, std::move(callback));
}

void RpcBroker::UnregisterMessageReceiverCallback(RpcBroker::Handle handle) {
  OSP_DVLOG << "unregistering handle: " << handle;
  receive_callbacks_.erase_key(handle);
}

void RpcBroker::ProcessMessageFromRemote(const RpcMessage& message) {
  OSP_DVLOG << "received message: " << message;
  const auto entry = receive_callbacks_.find(message.handle());
  if (entry == receive_callbacks_.end()) {
    OSP_DVLOG << "unregistered handle: " << message.handle();
    return;
  }
  entry->second(message);
}

void RpcBroker::SendMessageToRemote(const RpcMessage& message) {
  OSP_DVLOG << "sending message message: " << message;
  std::vector<uint8_t> serialized_message(message.ByteSizeLong());
  OSP_CHECK(message.SerializeToArray(serialized_message.data(),
                                     serialized_message.size()));
  send_message_cb_(std::move(serialized_message));
}

bool RpcBroker::IsRegisteredForTesting(RpcBroker::Handle handle) {
  return receive_callbacks_.find(handle) != receive_callbacks_.end();
}

}  // namespace cast
}  // namespace openscreen
