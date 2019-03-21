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
#include "base/memory/weak_ptr.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"

namespace remoting {

class FtlGrpcContext;

// A class for sending and receiving messages via the FTL API.
class FtlMessagingClient final {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const std::string& sender_id,
                                   const std::string& message)>;
  using MessageCallbackSubscription =
      base::CallbackList<void(const std::string&,
                              const std::string&)>::Subscription;
  using DoneCallback = base::OnceCallback<void(const grpc::Status& status)>;

  explicit FtlMessagingClient(FtlGrpcContext* context);
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

 private:
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  void OnPullMessagesResponse(DoneCallback on_done,
                              const grpc::Status& status,
                              const ftl::PullMessagesResponse& response);

  void AckMessages(const ftl::AckMessagesRequest& request,
                   DoneCallback on_done);

  void OnAckMessagesResponse(DoneCallback on_done,
                             const grpc::Status& status,
                             const ftl::AckMessagesResponse& response);

  void RunMessageCallbacks(const ftl::InboxMessage& message);

  FtlGrpcContext* context_;
  std::unique_ptr<Messaging::Stub> messaging_stub_;
  base::CallbackList<void(const std::string&, const std::string&)>
      callback_list_;

  base::WeakPtrFactory<FtlMessagingClient> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FtlMessagingClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_MESSAGING_CLIENT_H_
