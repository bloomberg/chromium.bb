// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_messenger.h"

#include "absl/strings/ascii.h"
#include "cast/common/public/message_port.h"
#include "cast/streaming/message_fields.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

namespace {

void ReplyIfTimedOut(
    int sequence_number,
    ReceiverMessage::Type reply_type,
    std::vector<std::pair<int, SenderSessionMessenger::ReplyCallback>>*
        replies) {
  for (auto it = replies->begin(); it != replies->end(); ++it) {
    if (it->first == sequence_number) {
      OSP_VLOG
          << "Replying with empty message due to timeout for sequence number: "
          << sequence_number;
      it->second(ReceiverMessage{reply_type, sequence_number});
      replies->erase(it);
      break;
    }
  }
}

}  // namespace

SessionMessenger::SessionMessenger(MessagePort* message_port,
                                   std::string source_id,
                                   ErrorCallback cb)
    : message_port_(message_port), error_callback_(std::move(cb)) {
  OSP_DCHECK(message_port_);
  OSP_DCHECK(!source_id.empty());
  message_port_->SetClient(this, source_id);
}

SessionMessenger::~SessionMessenger() {
  message_port_->ResetClient();
}

Error SessionMessenger::SendMessage(const std::string& destination_id,
                                    const std::string& namespace_,
                                    const Json::Value& message_root) {
  OSP_DCHECK(namespace_ == kCastRemotingNamespace ||
             namespace_ == kCastWebrtcNamespace);
  auto body_or_error = json::Stringify(message_root);
  if (body_or_error.is_error()) {
    return std::move(body_or_error.error());
  }
  OSP_VLOG << "Sending message: DESTINATION[" << destination_id
           << "], NAMESPACE[" << namespace_ << "], BODY:\n"
           << body_or_error.value();
  message_port_->PostMessage(destination_id, namespace_, body_or_error.value());
  return Error::None();
}

void SessionMessenger::ReportError(Error error) {
  error_callback_(std::move(error));
}

SenderSessionMessenger::SenderSessionMessenger(MessagePort* message_port,
                                               std::string source_id,
                                               std::string receiver_id,
                                               ErrorCallback cb,
                                               TaskRunner* task_runner)
    : SessionMessenger(message_port, std::move(source_id), std::move(cb)),
      task_runner_(task_runner),
      receiver_id_(std::move(receiver_id)) {}

void SenderSessionMessenger::SetHandler(ReceiverMessage::Type type,
                                        ReplyCallback cb) {
  // Currently the only handler allowed is for RPC messages.
  OSP_DCHECK(type == ReceiverMessage::Type::kRpc);
  rpc_callback_ = std::move(cb);
}

Error SenderSessionMessenger::SendOutboundMessage(SenderMessage message) {
  const auto namespace_ = (message.type == SenderMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  ErrorOr<Json::Value> jsonified = message.ToJson();
  OSP_CHECK(jsonified.is_value()) << "Tried to send an invalid message";
  return SessionMessenger::SendMessage(receiver_id_, namespace_,
                                       jsonified.value());
}

Error SenderSessionMessenger::SendRequest(SenderMessage message,
                                          ReceiverMessage::Type reply_type,
                                          ReplyCallback cb) {
  static constexpr std::chrono::milliseconds kReplyTimeout{4000};
  // RPC messages are not meant to be request/reply.
  OSP_DCHECK(reply_type != ReceiverMessage::Type::kRpc);

  const Error error = SendOutboundMessage(message);
  if (!error.ok()) {
    return error;
  }

  OSP_DCHECK(awaiting_replies_.find(message.sequence_number) ==
             awaiting_replies_.end());
  awaiting_replies_.emplace_back(message.sequence_number, std::move(cb));
  task_runner_->PostTaskWithDelay(
      [self = weak_factory_.GetWeakPtr(), reply_type,
       seq_num = message.sequence_number] {
        if (self) {
          ReplyIfTimedOut(seq_num, reply_type, &self->awaiting_replies_);
        }
      },
      kReplyTimeout);

  return Error::None();
}

void SenderSessionMessenger::OnMessage(const std::string& source_id,
                                       const std::string& message_namespace,
                                       const std::string& message) {
  if (source_id != receiver_id_) {
    OSP_DLOG_WARN << "Received message from unknown/incorrect Cast Receiver, "
                     "expected id \""
                  << receiver_id_ << "\", got \"" << source_id << "\"";
    return;
  }

  if (message_namespace != kCastWebrtcNamespace &&
      message_namespace != kCastRemotingNamespace) {
    OSP_DLOG_WARN << "Received message from unknown namespace: "
                  << message_namespace;
    return;
  }

  ErrorOr<Json::Value> message_body = json::Parse(message);
  if (!message_body) {
    ReportError(message_body.error());
    OSP_DLOG_WARN << "Received an invalid message: " << message;
    return;
  }

  // If the message is valid JSON and we don't understand it, there are two
  // options: (1) it's an unknown type, or (2) the receiver filled out the
  // message incorrectly. In the first case we can drop it, it's likely just
  // unsupported. In the second case we might need it, so worth warning the
  // client.
  ErrorOr<ReceiverMessage> receiver_message =
      ReceiverMessage::Parse(message_body.value());
  if (receiver_message.is_error()) {
    ReportError(receiver_message.error());
    OSP_DLOG_WARN << "Received an invalid receiver message: "
                  << receiver_message.error();
  }

  if (receiver_message.value().type == ReceiverMessage::Type::kRpc) {
    if (rpc_callback_) {
      rpc_callback_(receiver_message.value({}));
    } else {
      OSP_DLOG_INFO << "Received RTP message but no callback, dropping";
    }
  } else {
    int sequence_number;
    if (!json::TryParseInt(message_body.value()[kSequenceNumber],
                           &sequence_number)) {
      OSP_DLOG_WARN << "Received a message without a sequence number";
      return;
    }

    auto it = awaiting_replies_.find(sequence_number);
    if (it == awaiting_replies_.end()) {
      OSP_DLOG_WARN << "Received a reply I wasn't waiting for: "
                    << sequence_number;
      return;
    }

    it->second(std::move(receiver_message.value({})));

    // Calling the function callback may result in the checksum of the pointed
    // to object to change, so calling erase() on the iterator after executing
    // second() may result in a segfault.
    awaiting_replies_.erase_key(sequence_number);
  }
}

void SenderSessionMessenger::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messenger: " << error;
}

ReceiverSessionMessenger::ReceiverSessionMessenger(MessagePort* message_port,
                                                   std::string source_id,
                                                   ErrorCallback cb)
    : SessionMessenger(message_port, std::move(source_id), std::move(cb)) {}

void ReceiverSessionMessenger::SetHandler(SenderMessage::Type type,
                                          RequestCallback cb) {
  OSP_DCHECK(callbacks_.find(type) == callbacks_.end());
  callbacks_.emplace_back(type, std::move(cb));
}

Error ReceiverSessionMessenger::SendMessage(ReceiverMessage message) {
  if (sender_session_id_.empty()) {
    return Error(Error::Code::kInitializationFailure,
                 "Tried to send a message without receiving one first");
  }

  const auto namespace_ = (message.type == ReceiverMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  ErrorOr<Json::Value> message_json = message.ToJson();
  OSP_CHECK(message_json.is_value()) << "Tried to send an invalid message";
  return SessionMessenger::SendMessage(sender_session_id_, namespace_,
                                       message_json.value());
}

void ReceiverSessionMessenger::OnMessage(const std::string& source_id,
                                         const std::string& message_namespace,
                                         const std::string& message) {
  // We assume we are connected to the first sender_id we receive.
  if (sender_session_id_.empty()) {
    sender_session_id_ = source_id;
  } else if (source_id != sender_session_id_) {
    OSP_DLOG_WARN << "Received message from unknown/incorrect sender, expected "
                     "id \""
                  << sender_session_id_ << "\", got \"" << source_id << "\"";
    return;
  }

  if (message_namespace != kCastWebrtcNamespace &&
      message_namespace != kCastRemotingNamespace) {
    OSP_DLOG_WARN << "Received message from unknown namespace: "
                  << message_namespace;
    return;
  }

  // If the message is bad JSON, the sender is in a funky state so we
  // report an error.
  ErrorOr<Json::Value> message_body = json::Parse(message);
  if (message_body.is_error()) {
    ReportError(message_body.error());
    return;
  }

  // If the message is valid JSON and we don't understand it, there are two
  // options: (1) it's an unknown type, or (2) the sender filled out the message
  // incorrectly. In the first case we can drop it, it's likely just
  // unsupported. In the second case we might need it, so worth warning the
  // client.
  ErrorOr<SenderMessage> sender_message =
      SenderMessage::Parse(message_body.value());
  if (sender_message.is_error()) {
    ReportError(sender_message.error());
    OSP_DLOG_WARN << "Received an invalid sender message: "
                  << sender_message.error();
    return;
  }

  auto it = callbacks_.find(sender_message.value().type);
  if (it == callbacks_.end()) {
    OSP_DLOG_INFO << "Received message without a callback, dropping";
  } else {
    it->second(sender_message.value());
  }
}

void ReceiverSessionMessenger::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messenger: " << error;
}

}  // namespace cast
}  // namespace openscreen
