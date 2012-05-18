// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps an invalidation
// client.  Handles the details of connecting to XMPP and hooking it
// up to the invalidation client.
//
// You probably don't want to use this directly; use
// NonBlockingInvalidationNotifier.

#ifndef SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
#define SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/communicator/login.h"
#include "sync/notifier/chrome_invalidation_client.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/state_writer.h"
#include "sync/notifier/sync_notifier.h"
#include "sync/syncable/model_type.h"
#include "sync/util/weak_handle.h"

namespace sync_notifier {

// This class must live on the IO thread.
class InvalidationNotifier
    : public SyncNotifier,
      public notifier::LoginDelegate,
      public ChromeInvalidationClient::Listener,
      public StateWriter {
 public:
  // |invalidation_state_tracker| must be initialized.
  InvalidationNotifier(
      const notifier::NotifierOptions& notifier_options,
      const InvalidationVersionMap& initial_max_invalidation_versions,
      const browser_sync::WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      const std::string& client_info);

  virtual ~InvalidationNotifier();

  // SyncNotifier implementation.
  virtual void AddObserver(SyncNotifierObserver* observer) OVERRIDE;
  virtual void RemoveObserver(SyncNotifierObserver* observer) OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void SetState(const std::string& state) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void UpdateEnabledTypes(
      syncable::ModelTypeSet enabled_types) OVERRIDE;
  virtual void SendNotification(
      syncable::ModelTypeSet changed_types) OVERRIDE;

  // notifier::LoginDelegate implementation.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task) OVERRIDE;
  virtual void OnDisconnect() OVERRIDE;

  // ChromeInvalidationClient::Listener implementation.
  virtual void OnInvalidate(
      const syncable::ModelTypePayloadMap& type_payloads) OVERRIDE;
  virtual void OnSessionStatusChanged(bool has_session) OVERRIDE;

  // StateWriter implementation.
  virtual void WriteState(const std::string& state) OVERRIDE;

 private:
  base::NonThreadSafe non_thread_safe_;

  // We start off in the STOPPED state.  When we get our initial
  // credentials, we connect and move to the CONNECTING state.  When
  // we're connected we start the invalidation client and move to the
  // STARTED state.  We never go back to a previous state.
  enum State {
    STOPPED,
    CONNECTING,
    STARTED
  };
  State state_;

  // Used to build parameters for |login_|.
  const notifier::NotifierOptions notifier_options_;

  // Passed to |invalidation_client_|.
  const InvalidationVersionMap initial_max_invalidation_versions_;

  // Passed to |invalidation_client_|.
  const browser_sync::WeakHandle<InvalidationStateTracker>
      invalidation_state_tracker_;

  // Passed to |invalidation_client_|.
  const std::string client_info_;

  // Our observers (which must live on the same thread).
  ObserverList<SyncNotifierObserver> observers_;

  // The client ID to pass to |chrome_invalidation_client_|.
  std::string invalidation_client_id_;

  // The state to pass to |chrome_invalidation_client_|.
  std::string invalidation_state_;

  // The XMPP connection manager.
  // TODO(akalin): Use PushClient instead.
  scoped_ptr<notifier::Login> login_;

  // The invalidation client.
  ChromeInvalidationClient invalidation_client_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationNotifier);
};

}  // namespace sync_notifier

#endif  // SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
