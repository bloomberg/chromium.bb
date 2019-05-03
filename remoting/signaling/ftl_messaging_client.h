// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
#define REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_services.grpc.pb.h"

namespace remoting {

class GrpcExecutor;
class MessageReceptionChannel;
class OAuthTokenGetter;
class RegistrationManager;
class ScopedGrpcServerStream;

// A class for sending and receiving messages via the FTL API.
class FtlMessagingClient final {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const std::string& sender_id,
                                   const std::string& sender_registration_id,
                                   const ftl::ChromotingMessage& message)>;
  using MessageCallbackList =
      base::CallbackList<void(const std::string&,
                              const std::string&,
                              const ftl::ChromotingMessage&)>;
  using MessageCallbackSubscription = MessageCallbackList::Subscription;
  using DoneCallback = base::OnceCallback<void(const grpc::Status& status)>;

  // |token_getter| and |registration_manager| must outlive |this|.
  FtlMessagingClient(OAuthTokenGetter* token_getter,
                     RegistrationManager* registration_manager);
  ~FtlMessagingClient();

  // Registers a callback which is run for each new message received.
  // Simply delete the returned subscription object to unregister. The
  // subscription object must be deleted before |this| is deleted.
  std::unique_ptr<MessageCallbackSubscription> RegisterMessageCallback(
      const MessageCallback& callback);

  // Retrieves messages from the user's inbox over slow path and calls the
  // registered MessageCallback on every received message.
  // |on_done| is called once the messages have been received and acked on the
  // server's inbox.
  void PullMessages(DoneCallback on_done);
  void SendMessage(const std::string& destination,
                   const std::string& destination_registration_id,
                   const ftl::ChromotingMessage& message,
                   DoneCallback on_done);

  // Opens a stream to continuously receive new messages from the server and
  // calls the registered MessageCallback once a new message is received.
  // |on_ready| is called once the stream is successfully started.
  // |on_closed| is called if the stream fails to start, in which case
  // |on_ready| will not be called, or when the stream is closed or dropped,
  // in which case it is called after |on_ready| is called.
  void StartReceivingMessages(base::OnceClosure on_ready,
                              DoneCallback on_closed);

  // Stops the stream for continuously receiving new messages.
  void StopReceivingMessages();

  // Returns true if the streaming channel is open.
  bool IsReceivingMessages();

 private:
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  friend class FtlMessagingClientTest;

  FtlMessagingClient(std::unique_ptr<GrpcExecutor> executor,
                     RegistrationManager* registration_manager,
                     std::unique_ptr<MessageReceptionChannel> channel);

  void OnPullMessagesResponse(DoneCallback on_done,
                              const grpc::Status& status,
                              const ftl::PullMessagesResponse& response);

  void OnSendMessageResponse(DoneCallback on_done,
                             const grpc::Status& status,
                             const ftl::InboxSendResponse& response);

  void AckMessages(const ftl::AckMessagesRequest& request,
                   DoneCallback on_done);

  void OnAckMessagesResponse(DoneCallback on_done,
                             const grpc::Status& status,
                             const ftl::AckMessagesResponse& response);

  std::unique_ptr<ScopedGrpcServerStream> OpenReceiveMessagesStream(
      const base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>&
          on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed);

  void RunMessageCallbacks(const ftl::InboxMessage& message);

  void OnMessageReceived(const ftl::InboxMessage& message);

  std::unique_ptr<GrpcExecutor> executor_;
  RegistrationManager* registration_manager_;
  std::unique_ptr<Messaging::Stub> messaging_stub_;
  std::unique_ptr<MessageReceptionChannel> reception_channel_;
  MessageCallbackList callback_list_;

  DISALLOW_COPY_AND_ASSIGN(FtlMessagingClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
