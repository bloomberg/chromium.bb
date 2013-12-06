// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_PUSH_CLIENT_CHANNEL_H_
#define SYNC_NOTIFIER_PUSH_CLIENT_CHANNEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "sync/base/sync_export.h"
#include "sync/notifier/sync_system_resources.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

// A PushClientChannel is an implementation of NetworkChannel that
// routes messages through a PushClient.
class SYNC_EXPORT_PRIVATE PushClientChannel
    : public SyncNetworkChannel,
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

  // SyncNetworkChannel implementation.
  virtual void SendEncodedMessage(const std::string& encoded_message) OVERRIDE;

  // notifier::PushClient::Observer implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;

 private:
  scoped_ptr<notifier::PushClient> push_client_;

  DISALLOW_COPY_AND_ASSIGN(PushClientChannel);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_PUSH_CLIENT_CHANNEL_H_
