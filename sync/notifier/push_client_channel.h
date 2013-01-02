// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_PUSH_CLIENT_CHANNEL_H_
#define SYNC_NOTIFIER_PUSH_CLIENT_CHANNEL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "sync/base/sync_export.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

// A PushClientChannel is an implementation of NetworkChannel that
// routes messages through a PushClient.
class SYNC_EXPORT_PRIVATE PushClientChannel
    : public NON_EXPORTED_BASE(invalidation::NetworkChannel),
      public NON_EXPORTED_BASE(notifier::PushClientObserver) {
 public:
  // |push_client| is guaranteed to be destroyed only when this object
  // is destroyed.
  explicit PushClientChannel(scoped_ptr<notifier::PushClient> push_client);

  virtual ~PushClientChannel();

  // If not connected, connects with the given credentials.  If
  // already connected, the next connection attempt will use the given
  // credentials.
  void UpdateCredentials(const std::string& email, const std::string& token);

  // invalidation::NetworkChannel implementation.
  virtual void SendMessage(const std::string& outgoing_message) OVERRIDE;
  virtual void SetMessageReceiver(
      invalidation::MessageCallback* incoming_receiver) OVERRIDE;
  virtual void AddNetworkStatusReceiver(
      invalidation::NetworkStatusCallback* network_status_receiver) OVERRIDE;
  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;

  // notifier::PushClient::Observer implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;

  const std::string& GetServiceContextForTest() const;

  int64 GetSchedulingHashForTest() const;

  static notifier::Notification EncodeMessageForTest(
      const std::string& message,
      const std::string& service_context,
      int64 scheduling_hash);

  static bool DecodeMessageForTest(
      const notifier::Notification& notification,
      std::string* message,
      std::string* service_context,
      int64* scheduling_hash);

 private:
  typedef std::vector<invalidation::NetworkStatusCallback*>
      NetworkStatusReceiverList;

  static notifier::Notification EncodeMessage(
      const std::string& message,
      const std::string& service_context,
      int64 scheduling_hash);

  static bool DecodeMessage(
      const notifier::Notification& notification,
      std::string* message,
      std::string* service_context,
      int64* scheduling_hash);

  scoped_ptr<notifier::PushClient> push_client_;
  scoped_ptr<invalidation::MessageCallback> incoming_receiver_;
  NetworkStatusReceiverList network_status_receivers_;

  bool notifications_enabled_;

  // Service context.
  std::string service_context_;

  // Scheduling hash.
  int64 scheduling_hash_;

  DISALLOW_COPY_AND_ASSIGN(PushClientChannel);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_PUSH_CLIENT_CHANNEL_H_
