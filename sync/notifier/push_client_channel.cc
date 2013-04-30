// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/push_client_channel.h"

#include "base/stl_util.h"
#include "google/cacheinvalidation/client_gateway.pb.h"
#include "jingle/notifier/listener/push_client.h"

namespace syncer {

namespace {

const char kBotJid[] = "tango@bot.talk.google.com";
const char kChannelName[] = "tango_raw";

}  // namespace

PushClientChannel::PushClientChannel(
    scoped_ptr<notifier::PushClient> push_client)
    : push_client_(push_client.Pass()),
      notifications_enabled_(false),
      scheduling_hash_(0) {
  push_client_->AddObserver(this);
  notifier::Subscription subscription;
  subscription.channel = kChannelName;
  subscription.from = "";
  notifier::SubscriptionList subscriptions;
  subscriptions.push_back(subscription);
  push_client_->UpdateSubscriptions(subscriptions);
}

PushClientChannel::~PushClientChannel() {
  push_client_->RemoveObserver(this);
  STLDeleteElements(&network_status_receivers_);
}

void PushClientChannel::UpdateCredentials(
    const std::string& email, const std::string& token) {
  push_client_->UpdateCredentials(email, token);
}

void PushClientChannel::SendMessage(const std::string& outgoing_message) {
  push_client_->SendNotification(
      EncodeMessage(outgoing_message, service_context_, scheduling_hash_));
}

void PushClientChannel::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  incoming_receiver_.reset(incoming_receiver);
}

void PushClientChannel::AddNetworkStatusReceiver(
    invalidation::NetworkStatusCallback* network_status_receiver) {
  network_status_receiver->Run(notifications_enabled_);
  network_status_receivers_.push_back(network_status_receiver);
}

void PushClientChannel::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void PushClientChannel::OnNotificationsEnabled() {
  for (NetworkStatusReceiverList::const_iterator it =
           network_status_receivers_.begin();
       it != network_status_receivers_.end(); ++it) {
    (*it)->Run(true);
  }
}

void PushClientChannel::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  for (NetworkStatusReceiverList::const_iterator it =
           network_status_receivers_.begin();
       it != network_status_receivers_.end(); ++it) {
    (*it)->Run(false);
  }
}

void PushClientChannel::OnIncomingNotification(
    const notifier::Notification& notification) {
  if (!incoming_receiver_) {
    DLOG(ERROR) << "No receiver for incoming notification";
    return;
  }
  std::string message;
  if (!DecodeMessage(notification,
                     &message, &service_context_, &scheduling_hash_)) {
    DLOG(ERROR) << "Could not parse ClientGatewayMessage from: "
                << notification.ToString();
  }
  incoming_receiver_->Run(message);
}

const std::string& PushClientChannel::GetServiceContextForTest() const {
  return service_context_;
}

int64 PushClientChannel::GetSchedulingHashForTest() const {
  return scheduling_hash_;
}

notifier::Notification PushClientChannel::EncodeMessageForTest(
    const std::string& message, const std::string& service_context,
    int64 scheduling_hash) {
  return EncodeMessage(message, service_context, scheduling_hash);
}

bool PushClientChannel::DecodeMessageForTest(
    const notifier::Notification& notification,
    std::string* message,
    std::string* service_context,
    int64* scheduling_hash) {
  return DecodeMessage(
      notification, message, service_context, scheduling_hash);
}

notifier::Notification PushClientChannel::EncodeMessage(
    const std::string& message, const std::string& service_context,
    int64 scheduling_hash) {
  ipc::invalidation::ClientGatewayMessage envelope;
  envelope.set_is_client_to_server(true);
  if (!service_context.empty()) {
    envelope.set_service_context(service_context);
    envelope.set_rpc_scheduling_hash(scheduling_hash);
  }
  envelope.set_network_message(message);

  notifier::Recipient recipient;
  recipient.to = kBotJid;
  notifier::Notification notification;
  notification.channel = kChannelName;
  notification.recipients.push_back(recipient);
  envelope.SerializeToString(&notification.data);
  return notification;
}

bool PushClientChannel::DecodeMessage(
    const notifier::Notification& notification,
    std::string* message,
    std::string* service_context,
    int64* scheduling_hash) {
  ipc::invalidation::ClientGatewayMessage envelope;
  if (!envelope.ParseFromString(notification.data)) {
    return false;
  }
  *message = envelope.network_message();
  if (envelope.has_service_context()) {
    *service_context = envelope.service_context();
  }
  if (envelope.has_rpc_scheduling_hash()) {
    *scheduling_hash = envelope.rpc_scheduling_hash();
  }
  return true;
}

}  // namespace syncer
