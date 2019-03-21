// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "remoting/signaling/ftl_grpc_context.h"

namespace remoting {

namespace {

void AddMessageToAckRequest(const ftl::InboxMessage& message,
                            ftl::AckMessagesRequest* request) {
  ftl::ReceiverMessage* receiver_message = request->add_messages();
  receiver_message->set_message_id(message.message_id());
  receiver_message->set_allocated_receiver_id(
      new ftl::Id(message.receiver_id()));
}

}  // namespace

FtlMessagingClient::FtlMessagingClient(FtlGrpcContext* context)
    : weak_factory_(this) {
  context_ = context;
  messaging_stub_ = Messaging::NewStub(context_->channel());
}

FtlMessagingClient::~FtlMessagingClient() = default;

std::unique_ptr<FtlMessagingClient::MessageCallbackSubscription>
FtlMessagingClient::RegisterMessageCallback(const MessageCallback& callback) {
  return callback_list_.Add(callback);
}

void FtlMessagingClient::PullMessages(DoneCallback on_done) {
  context_->ExecuteRpc(
      base::BindOnce(&Messaging::Stub::AsyncPullMessages,
                     base::Unretained(messaging_stub_.get())),
      ftl::PullMessagesRequest(),
      base::BindOnce(&FtlMessagingClient::OnPullMessagesResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlMessagingClient::OnPullMessagesResponse(
    DoneCallback on_done,
    const grpc::Status& status,
    const ftl::PullMessagesResponse& response) {
  if (!status.ok()) {
    LOG(ERROR) << "Failed to pull messages. "
               << "Error code: " << status.error_code()
               << ", message: " << status.error_message();
    std::move(on_done).Run(status);
    return;
  }

  ftl::AckMessagesRequest ack_request;
  for (const auto& message : response.messages()) {
    RunMessageCallbacks(message);
    AddMessageToAckRequest(message, &ack_request);
  }

  if (ack_request.messages_size() == 0) {
    LOG(WARNING) << "No new message is received.";
    std::move(on_done).Run(status);
    return;
  }

  // TODO(yuweih): May need retry logic.
  VLOG(0) << "Acking " << ack_request.messages_size() << " messages";

  AckMessages(ack_request, std::move(on_done));
}

void FtlMessagingClient::AckMessages(const ftl::AckMessagesRequest& request,
                                     DoneCallback on_done) {
  context_->ExecuteRpc(
      base::BindOnce(&Messaging::Stub::AsyncAckMessages,
                     base::Unretained(messaging_stub_.get())),
      request,
      base::BindOnce(&FtlMessagingClient::OnAckMessagesResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlMessagingClient::OnAckMessagesResponse(
    DoneCallback on_done,
    const grpc::Status& status,
    const ftl::AckMessagesResponse& response) {
  std::move(on_done).Run(status);
}

void FtlMessagingClient::RunMessageCallbacks(const ftl::InboxMessage& message) {
  if (message.message_type() !=
      ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE) {
    LOG(WARNING) << "Received message with unknown type: "
                 << message.message_type()
                 << ", sender: " << message.sender_id().id();
    return;
  }

  ftl::ChromotingMessage chromoting_message;
  chromoting_message.ParseFromString(message.message());
  callback_list_.Notify(message.sender_id().id(), chromoting_message.message());
}

}  // namespace remoting
