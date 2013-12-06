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
    : push_client_(push_client.Pass()) {
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
}

void PushClientChannel::UpdateCredentials(
    const std::string& email, const std::string& token) {
  push_client_->UpdateCredentials(email, token);
}

void PushClientChannel::SendEncodedMessage(const std::string& encoded_message) {
  notifier::Recipient recipient;
  recipient.to = kBotJid;
  notifier::Notification notification;
  notification.channel = kChannelName;
  notification.recipients.push_back(recipient);
  notification.data = encoded_message;
  push_client_->SendNotification(notification);
}

void PushClientChannel::OnNotificationsEnabled() {
  NotifyStateChange(INVALIDATIONS_ENABLED);
}

void PushClientChannel::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  NotifyStateChange(FromNotifierReason(reason));
}

void PushClientChannel::OnIncomingNotification(
    const notifier::Notification& notification) {
  DeliverIncomingMessage(notification.data);
}

}  // namespace syncer
