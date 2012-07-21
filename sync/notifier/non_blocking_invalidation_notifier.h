// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps InvalidationNotifier
// on its own thread.

#ifndef SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
#define SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "jingle/notifier/base/notifier_options.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/sync_notifier.h"
#include "sync/notifier/sync_notifier_helper.h"
#include "sync/notifier/sync_notifier_observer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace syncer {

class NonBlockingInvalidationNotifier
    : public SyncNotifier,
      // SyncNotifierObserver to "observe" our Core via WeakHandle.
      public SyncNotifierObserver {
 public:
  // |invalidation_state_tracker| must be initialized.
  NonBlockingInvalidationNotifier(
      const notifier::NotifierOptions& notifier_options,
      const InvalidationVersionMap& initial_max_invalidation_versions,
      const std::string& initial_invalidation_state,
      const WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      const std::string& client_info);

  virtual ~NonBlockingInvalidationNotifier();

  // SyncNotifier implementation.
  virtual void UpdateRegisteredIds(SyncNotifierObserver* handler,
                                   const ObjectIdSet& ids) OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void SetStateDeprecated(const std::string& state) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendNotification(ModelTypeSet changed_types) OVERRIDE;

  // SyncNotifierObserver implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const ObjectIdPayloadMap& id_payloads,
      IncomingNotificationSource source) OVERRIDE;

 private:
  class Core;

  base::WeakPtrFactory<NonBlockingInvalidationNotifier> weak_ptr_factory_;

  SyncNotifierHelper helper_;

  // The real guts of NonBlockingInvalidationNotifier, which allows
  // this class to live completely on the parent thread.
  scoped_refptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingInvalidationNotifier);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
