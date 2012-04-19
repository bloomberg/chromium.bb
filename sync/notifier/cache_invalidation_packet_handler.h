// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Class that handles the details of sending and receiving client
// invalidation packets.

#ifndef SYNC_NOTIFIER_CACHE_INVALIDATION_PACKET_HANDLER_H_
#define SYNC_NOTIFIER_CACHE_INVALIDATION_PACKET_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace sync_notifier {

class CacheInvalidationPacketHandler
    : public notifier::PushNotificationsSubscribeTaskDelegate,
      public notifier::PushNotificationsListenTaskDelegate {
 public:
  // Starts routing packets from |invalidation_client| using
  // |base_task|.  |base_task.get()| must still be non-NULL.
  // |invalidation_client| must not already be routing packets through
  // something.  Does not take ownership of |invalidation_client|.
  CacheInvalidationPacketHandler(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task);

  // Makes the invalidation client passed into the constructor not
  // route packets through the XMPP client passed into the constructor
  // anymore.
  virtual ~CacheInvalidationPacketHandler();

  // If |base_task| is non-NULL, sends the outgoing message.
  virtual void SendMessage(const std::string& outgoing_message);

  virtual void SetMessageReceiver(
      invalidation::MessageCallback* incoming_receiver);

  // Sends a message requesting a subscription to the notification channel.
  virtual void SendSubscriptionRequest();

  // PushNotificationsSubscribeTaskDelegate implementation.
  virtual void OnSubscribed() OVERRIDE;
  virtual void OnSubscriptionError() OVERRIDE;

  // PushNotificationsListenTaskDelegate implementation.
  virtual void OnNotificationReceived(
      const notifier::Notification& notification) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(CacheInvalidationPacketHandlerTest, Basic);

  base::NonThreadSafe non_thread_safe_;
  base::WeakPtrFactory<CacheInvalidationPacketHandler> weak_factory_;

  base::WeakPtr<buzz::XmppTaskParentInterface> base_task_;

  scoped_ptr<invalidation::MessageCallback> incoming_receiver_;

  // Parameters for sent messages.

  // Monotonically increasing sequence number.
  int seq_;
  // Service context.
  std::string service_context_;
  // Scheduling hash.
  int64 scheduling_hash_;

  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationPacketHandler);
};

}  // namespace sync_notifier

#endif  // SYNC_NOTIFIER_CACHE_INVALIDATION_PACKET_HANDLER_H_
