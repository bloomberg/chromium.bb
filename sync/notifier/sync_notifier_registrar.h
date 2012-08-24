// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_SYNC_NOTIFIER_REGISTRAR_H_
#define SYNC_NOTIFIER_SYNC_NOTIFIER_REGISTRAR_H_

#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/object_id_state_map.h"
#include "sync/notifier/sync_notifier_observer.h"

namespace invalidation {
class ObjectId;
}  // namespace invalidation

namespace syncer {

// A helper class for implementations of the SyncNotifier interface.  It helps
// keep track of registered handlers and which object ID registrations are
// associated with which handlers, so implementors can just reuse the logic
// here to dispatch invalidations and other interesting notifications.
class SyncNotifierRegistrar {
 public:
  SyncNotifierRegistrar();
  ~SyncNotifierRegistrar();

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must already be registered.
  void RegisterHandler(SyncNotifierObserver* handler);


  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler.
  void UpdateRegisteredIds(SyncNotifierObserver* handler,
                           const ObjectIdSet& ids);

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  void UnregisterHandler(SyncNotifierObserver* handler);

  // Returns the set of all IDs that are registered to some handler (even
  // handlers that have been unregistered).
  ObjectIdSet GetAllRegisteredIds() const;

  // Sorts incoming invalidations into a bucket for each handler and then
  // dispatches the batched invalidations to the corresponding handler.
  // Invalidations for IDs with no corresponding handler are dropped, as are
  // invalidations for handlers that are not added.
  void DispatchInvalidationsToHandlers(const ObjectIdStateMap& id_state_map,
                                       IncomingNotificationSource source);

  // Calls the given handler method for each handler that has registered IDs.
  void EmitOnNotificationsEnabled();
  void EmitOnNotificationsDisabled(NotificationsDisabledReason reason);

  // Needed for death tests.
  void DetachFromThreadForTest();

 private:
  typedef std::map<invalidation::ObjectId, SyncNotifierObserver*,
                   ObjectIdLessThan>
      IdHandlerMap;

  SyncNotifierObserver* ObjectIdToHandler(const invalidation::ObjectId& id);

  base::ThreadChecker thread_checker_;
  ObserverList<SyncNotifierObserver> handlers_;
  IdHandlerMap id_to_handler_map_;

  DISALLOW_COPY_AND_ASSIGN(SyncNotifierRegistrar);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_SYNC_NOTIFIER_REGISTRAR_H_
