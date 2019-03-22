// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "remoting/signaling/ftl_grpc_context.h"
#include "remoting/signaling/ftl_message_reception_channel.h"

namespace remoting {

namespace {

void AddMessageToAckRequest(const ftl::InboxMessage& message,
                            ftl::AckMessagesRequest* request) {
  ftl::ReceiverMessage* receiver_message = request->add_messages();
  receiver_message->set_message_id(message.message_id());
  receiver_message->set_allocated_receiver_id(
      new ftl::Id(message.receiver_id()));
}

constexpr base::TimeDelta kInboxMessageTtl = base::TimeDelta::FromMinutes(1);

}  // namespace

FtlMessagingClient::FtlMessagingClient(FtlGrpcContext* context)
    : weak_factory_(this) {
  context_ = context;
  messaging_stub_ = Messaging::NewStub(context_->channel());
  reception_channel_ = std::make_unique<FtlMessageReceptionChannel>();
  reception_channel_->Initialize(
      base::BindRepeating(&FtlMessagingClient::OpenReceiveMessagesStream,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&FtlMessagingClient::OnMessageReceived,
                          weak_factory_.GetWeakPtr()));
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

void FtlMessagingClient::SendMessage(
    const std::string& destination,
    const std::string& destination_registration_id,
    const std::string& message_text,
    DoneCallback on_done) {
  ftl::InboxSendRequest request;
  request.set_time_to_live(kInboxMessageTtl.InMicroseconds());
  // TODO(yuweih): See if we need to set requester_id
  (*request.mutable_dest_id()) = FtlGrpcContext::BuildIdFromString(destination);

  ftl::ChromotingMessage crd_message;
  crd_message.set_message(message_text);
  std::string serialized_message;
  bool succeeded = crd_message.SerializeToString(&serialized_message);
  DCHECK(succeeded);

  request.mutable_message()->set_message(serialized_message);
  request.mutable_message()->set_message_id(base::GenerateGUID());
  request.mutable_message()->set_message_type(
      ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE);
  request.mutable_message()->set_message_class(
      ftl::InboxMessage_MessageClass_USER);
  if (!destination_registration_id.empty()) {
    request.add_dest_registration_ids(destination_registration_id);
  }

  context_->ExecuteRpc(
      base::BindOnce(&Messaging::Stub::AsyncSendMessage,
                     base::Unretained(messaging_stub_.get())),
      request,
      base::BindOnce(&FtlMessagingClient::OnSendMessageResponse,
                     weak_factory_.GetWeakPtr(), std::move(on_done)));
}

void FtlMessagingClient::StartReceivingMessages(DoneCallback on_done) {
  reception_channel_->StartReceivingMessages(std::move(on_done));
}

void FtlMessagingClient::StopReceivingMessages() {
  reception_channel_->StopReceivingMessages();
}

void FtlMessagingClient::SetMessageReceptionChannelForTesting(
    std::unique_ptr<MessageReceptionChannel> channel) {
  reception_channel_ = std::move(channel);
  reception_channel_->Initialize(
      base::BindRepeating(&FtlMessagingClient::OpenReceiveMessagesStream,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&FtlMessagingClient::OnMessageReceived,
                          weak_factory_.GetWeakPtr()));
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

  VLOG(0) << "Acking " << ack_request.messages_size() << " messages";

  AckMessages(ack_request, std::move(on_done));
}

void FtlMessagingClient::OnSendMessageResponse(
    DoneCallback on_done,
    const grpc::Status& status,
    const ftl::InboxSendResponse& response) {
  std::move(on_done).Run(status);
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
  // TODO(yuweih): Handle failure.
  std::move(on_done).Run(status);
}

void FtlMessagingClient::OpenReceiveMessagesStream(
    base::OnceCallback<void(std::unique_ptr<ScopedGrpcServerStream>)>
        on_stream_started,
    const base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>&
        on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed) {
  context_->ExecuteServerStreamingRpc(
      base::BindOnce(&Messaging::Stub::AsyncReceiveMessages,
                     base::Unretained(messaging_stub_.get())),
      ftl::ReceiveMessagesRequest(), std::move(on_stream_started),
      on_incoming_msg, std::move(on_channel_closed));
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

void FtlMessagingClient::OnMessageReceived(const ftl::InboxMessage& message) {
  RunMessageCallbacks(message);
  ftl::AckMessagesRequest ack_request;
  AddMessageToAckRequest(message, &ack_request);
  AckMessages(ack_request, base::DoNothing());
}

}  // namespace remoting
