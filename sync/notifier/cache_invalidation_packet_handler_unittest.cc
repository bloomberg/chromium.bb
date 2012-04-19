// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/cache_invalidation_packet_handler.h"

#include "base/base64.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "google/cacheinvalidation/deps/callback.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "google/cacheinvalidation/v2/client_gateway.pb.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "talk/base/task.h"
#include "talk/xmpp/asyncsocket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

class MockMessageCallback {
 public:
  void StoreMessage(const std::string& message) {
    last_message = message;
  }

  std::string last_message;
};

class CacheInvalidationPacketHandlerTest : public testing::Test {
 public:
  virtual ~CacheInvalidationPacketHandlerTest() {}

  notifier::Notification MakeNotification(const std::string& data) {
    notifier::Notification notification;
    notification.channel = "tango_raw";
    notification.data = data;
    return notification;
  }
};

TEST_F(CacheInvalidationPacketHandlerTest, Basic) {
  MessageLoop message_loop;

  notifier::FakeBaseTask fake_base_task;

  std::string last_message;
  MockMessageCallback callback;
  invalidation::MessageCallback* mock_message_callback =
      invalidation::NewPermanentCallback(
          &callback, &MockMessageCallback::StoreMessage);

  const char kInboundMessage[] = "non-bogus";
  ipc::invalidation::ClientGatewayMessage envelope;
  envelope.set_network_message(kInboundMessage);
  std::string serialized;
  envelope.SerializeToString(&serialized);
  {
    CacheInvalidationPacketHandler handler(fake_base_task.AsWeakPtr());
    handler.SetMessageReceiver(mock_message_callback);

    // Take care of any tasks posted by the constructor.
    message_loop.RunAllPending();

    {
      handler.OnNotificationReceived(MakeNotification("bogus"));
      handler.OnNotificationReceived(MakeNotification(serialized));
    }

    // Take care of any tasks posted by HandleOutboundPacket().
    message_loop.RunAllPending();

    EXPECT_EQ(callback.last_message, kInboundMessage);
  }
}

}  // namespace sync_notifier
