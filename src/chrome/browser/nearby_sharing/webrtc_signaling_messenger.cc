// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_signaling_messenger.h"

#include "base/callback_helpers.h"
#include "base/token.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : token_fetcher_(identity_manager),
      send_message_express_(&token_fetcher_, url_loader_factory),
      receive_messages_express_(&token_fetcher_, url_loader_factory) {}

WebRtcSignalingMessenger::~WebRtcSignalingMessenger() = default;

void WebRtcSignalingMessenger::SendMessage(const std::string& self_id,
                                           const std::string& peer_id,
                                           const std::string& message,
                                           SendMessageCallback callback) {
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request = BuildSendRequest(self_id, peer_id);

  chrome_browser_nearby_sharing_instantmessaging::InboxMessage* inbox_message =
      request.mutable_message();
  inbox_message->set_message_id(base::Token::CreateRandom().ToString());
  inbox_message->set_message(message);
  inbox_message->set_message_class(
      chrome_browser_nearby_sharing_instantmessaging::InboxMessage::EPHEMERAL);
  inbox_message->set_message_type(
      chrome_browser_nearby_sharing_instantmessaging::InboxMessage::BASIC);

  send_message_express_.SendMessage(request, std::move(callback));
}

void WebRtcSignalingMessenger::StartReceivingMessages(
    const std::string& self_id,
    mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
        incoming_messages_listener,
    StartReceivingMessagesCallback callback) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
      request = BuildReceiveRequest(self_id);

  incoming_messages_listener_.reset();
  incoming_messages_listener_.Bind(std::move(incoming_messages_listener));

  // base::Unretained is safe since |this| owns |receive_messages_express_|.
  receive_messages_express_.StartReceivingMessages(
      request,
      base::BindRepeating(&WebRtcSignalingMessenger::OnMessageReceived,
                          base::Unretained(this)),
      base::BindOnce(&WebRtcSignalingMessenger::OnStartedReceivingMessages,
                     base::Unretained(this), std::move(callback)));
}

void WebRtcSignalingMessenger::StopReceivingMessages() {
  incoming_messages_listener_.reset();
  receive_messages_express_.StopReceivingMessages();
}

void WebRtcSignalingMessenger::OnStartedReceivingMessages(
    StartReceivingMessagesCallback callback,
    bool success) {
  if (!success)
    incoming_messages_listener_.reset();

  std::move(callback).Run(success);
}

void WebRtcSignalingMessenger::OnMessageReceived(const std::string& message) {
  if (!incoming_messages_listener_)
    return;

  incoming_messages_listener_->OnMessage(message);
}
