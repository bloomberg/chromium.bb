// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of Invalidator that wraps an invalidation
// client.  Handles the details of connecting to XMPP and hooking it
// up to the invalidation client.
//
// You probably don't want to use this directly; use
// NonBlockingInvalidator.

#ifndef SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
#define SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/default_tick_clock.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_registrar.h"
#include "sync/notifier/sync_invalidation_listener.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

// This class must live on the IO thread.
// TODO(dcheng): Think of a name better than InvalidationInvalidator.
class SYNC_EXPORT_PRIVATE InvalidationNotifier
    : public Invalidator,
      public SyncInvalidationListener::Delegate,
      public base::NonThreadSafe {
 public:
  // |invalidation_state_tracker| must be initialized.
  InvalidationNotifier(
      scoped_ptr<notifier::PushClient> push_client,
      const InvalidationStateMap& initial_invalidation_state_map,
      const std::string& invalidation_bootstrap_data,
      const WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      const std::string& client_info);

  virtual ~InvalidationNotifier();

  // Invalidator implementation.
  virtual void RegisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(InvalidationHandler* handler) OVERRIDE;
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const AckHandle& ack_handle) OVERRIDE;
  virtual InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  // SyncInvalidationListener::Delegate implementation.
  virtual void OnInvalidate(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE;

 private:
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

  InvalidatorRegistrar registrar_;

  // Passed to |invalidation_listener_|.
  const InvalidationStateMap initial_invalidation_state_map_;

  // Passed to |invalidation_listener_|.
  const WeakHandle<InvalidationStateTracker>
      invalidation_state_tracker_;

  // Passed to |invalidation_listener_|.
  const std::string client_info_;

  // The client ID to pass to |invalidation_listener_|.
  std::string client_id_;

  // The initial bootstrap data to pass to |invalidation_listener_|.
  const std::string invalidation_bootstrap_data_;

  // TODO(akalin): Clean up this reference to DefaultTickClock. Ideally, we
  // should simply be using TaskRunner's tick clock. See http://crbug.com/179211
  base::DefaultTickClock tick_clock_;

  // The invalidation listener.
  SyncInvalidationListener invalidation_listener_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationNotifier);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_NOTIFIER_H_
