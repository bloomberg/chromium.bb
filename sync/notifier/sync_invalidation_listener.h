// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple wrapper around invalidation::InvalidationClient that
// handles all the startup/shutdown details and hookups.

#ifndef SYNC_NOTIFIER_SYNC_INVALIDATION_LISTENER_H_
#define SYNC_NOTIFIER_SYNC_INVALIDATION_LISTENER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "google/cacheinvalidation/include/invalidation-listener.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/object_id_invalidation_map.h"
#include "sync/notifier/state_writer.h"
#include "sync/notifier/sync_system_resources.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

class RegistrationManager;

// SyncInvalidationListener is not thread-safe and lives on the sync
// thread.
class SYNC_EXPORT_PRIVATE SyncInvalidationListener
    : public NON_EXPORTED_BASE(invalidation::InvalidationListener),
      public StateWriter,
      public NON_EXPORTED_BASE(notifier::PushClientObserver),
      public base::NonThreadSafe {
 public:
  typedef base::Callback<invalidation::InvalidationClient*(
      invalidation::SystemResources*,
      int,
      const invalidation::string&,
      const invalidation::string&,
      invalidation::InvalidationListener*)> CreateInvalidationClientCallback;

  class SYNC_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate();

    virtual void OnInvalidate(
        const ObjectIdInvalidationMap& invalidation_map) = 0;

    virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;
  };

  explicit SyncInvalidationListener(
      scoped_ptr<notifier::PushClient> push_client);

  // Calls Stop().
  virtual ~SyncInvalidationListener();

  // Does not take ownership of |delegate| or |state_writer|.
  // |invalidation_state_tracker| must be initialized.
  void Start(
      const CreateInvalidationClientCallback&
          create_invalidation_client_callback,
      const std::string& client_id, const std::string& client_info,
      const std::string& invalidation_bootstrap_data,
      const InvalidationStateMap& initial_invalidation_state_map,
      const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
      Delegate* delegate);

  void UpdateCredentials(const std::string& email, const std::string& token);

  // Update the set of object IDs that we're interested in getting
  // notifications for.  May be called at any time.
  void UpdateRegisteredIds(const ObjectIdSet& ids);

  // invalidation::InvalidationListener implementation.
  virtual void Ready(
      invalidation::InvalidationClient* client) OVERRIDE;
  virtual void Invalidate(
      invalidation::InvalidationClient* client,
      const invalidation::Invalidation& invalidation,
      const invalidation::AckHandle& ack_handle) OVERRIDE;
  virtual void InvalidateUnknownVersion(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      const invalidation::AckHandle& ack_handle) OVERRIDE;
  virtual void InvalidateAll(
      invalidation::InvalidationClient* client,
      const invalidation::AckHandle& ack_handle) OVERRIDE;
  virtual void InformRegistrationStatus(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      invalidation::InvalidationListener::RegistrationState reg_state) OVERRIDE;
  virtual void InformRegistrationFailure(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      bool is_transient,
      const std::string& error_message) OVERRIDE;
  virtual void ReissueRegistrations(
      invalidation::InvalidationClient* client,
      const std::string& prefix,
      int prefix_length) OVERRIDE;
  virtual void InformError(
      invalidation::InvalidationClient* client,
      const invalidation::ErrorInfo& error_info) OVERRIDE;

  // StateWriter implementation.
  virtual void WriteState(const std::string& state) OVERRIDE;

  // notifier::PushClientObserver implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;

  void DoRegistrationUpdate();

  void StopForTest();
  InvalidationStateMap GetStateMapForTest() const;

 private:
  void Stop();

  InvalidatorState GetState() const;

  void EmitStateChange();

  void EmitInvalidation(const ObjectIdInvalidationMap& invalidation_map);

  // Owned by |sync_system_resources_|.
  notifier::PushClient* const push_client_;
  SyncSystemResources sync_system_resources_;
  InvalidationStateMap invalidation_state_map_;
  WeakHandle<InvalidationStateTracker> invalidation_state_tracker_;
  Delegate* delegate_;
  scoped_ptr<invalidation::InvalidationClient> invalidation_client_;
  scoped_ptr<RegistrationManager> registration_manager_;
  // Stored to pass to |registration_manager_| on start.
  ObjectIdSet registered_ids_;

  // The states of the ticl and the push client.
  InvalidatorState ticl_state_;
  InvalidatorState push_client_state_;

  DISALLOW_COPY_AND_ASSIGN(SyncInvalidationListener);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_SYNC_INVALIDATION_LISTENER_H_
