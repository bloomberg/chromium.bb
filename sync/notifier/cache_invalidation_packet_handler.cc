// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/cache_invalidation_packet_handler.h"

#include <string>

#include "base/base64.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "google/cacheinvalidation/impl/constants.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "google/cacheinvalidation/v2/client_gateway.pb.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/push_notifications_send_update_task.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmpptask.h"

namespace sync_notifier {

namespace {

const char kBotJid[] = "tango@bot.talk.google.com";
const char kChannelName[] = "tango_raw";

}  // namespace

CacheInvalidationPacketHandler::CacheInvalidationPacketHandler(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task)
    : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      base_task_(base_task),
      seq_(0),
      scheduling_hash_(0) {
  CHECK(base_task_.get());
  // Owned by base_task.  Takes ownership of the callback.
  notifier::PushNotificationsListenTask* listen_task =
      new notifier::PushNotificationsListenTask(base_task_, this);
  listen_task->Start();
}

CacheInvalidationPacketHandler::~CacheInvalidationPacketHandler() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void CacheInvalidationPacketHandler::SendMessage(
    const std::string& message) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!base_task_.get()) {
    return;
  }
  ipc::invalidation::ClientGatewayMessage envelope;
  envelope.set_is_client_to_server(true);
  if (!service_context_.empty()) {
    envelope.set_service_context(service_context_);
    envelope.set_rpc_scheduling_hash(scheduling_hash_);
  }
  envelope.set_network_message(message);

  notifier::Recipient recipient;
  recipient.to = kBotJid;
  notifier::Notification notification;
  notification.channel = kChannelName;
  notification.recipients.push_back(recipient);
  envelope.SerializeToString(&notification.data);

  // Owned by base_task_.
  notifier::PushNotificationsSendUpdateTask* send_message_task =
      new notifier::PushNotificationsSendUpdateTask(base_task_, notification);
  send_message_task->Start();
}

void CacheInvalidationPacketHandler::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  incoming_receiver_.reset(incoming_receiver);
}

void CacheInvalidationPacketHandler::SendSubscriptionRequest() {
  notifier::Subscription subscription;
  subscription.channel = kChannelName;
  subscription.from = "";
  notifier::SubscriptionList subscription_list;
  subscription_list.push_back(subscription);
  // Owned by base_task_.
  notifier::PushNotificationsSubscribeTask* push_subscription_task =
      new notifier::PushNotificationsSubscribeTask(
          base_task_, subscription_list, this);
  push_subscription_task->Start();
}

void CacheInvalidationPacketHandler::OnSubscribed() {
  // TODO(ghc): Consider whether we should do more here.
}

void CacheInvalidationPacketHandler::OnSubscriptionError() {
  // TODO(ghc): Consider whether we should do more here.
}

void CacheInvalidationPacketHandler::OnNotificationReceived(
    const notifier::Notification& notification) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  const std::string& decoded_message = notification.data;
  ipc::invalidation::ClientGatewayMessage envelope;
  envelope.ParseFromString(decoded_message);
  if (!envelope.IsInitialized()) {
    LOG(ERROR) << "Could not parse ClientGatewayMessage: "
               << decoded_message;
    return;
  }
  if (envelope.has_service_context()) {
    service_context_ = envelope.service_context();
  }
  if (envelope.has_rpc_scheduling_hash()) {
    scheduling_hash_ = envelope.rpc_scheduling_hash();
  }
  incoming_receiver_->Run(envelope.network_message());
}

}  // namespace sync_notifier
