// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_SYNC_NOTIFIER_HELPER_H_
#define SYNC_NOTIFIER_SYNC_NOTIFIER_HELPER_H_

#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/object_id_payload_map.h"
#include "sync/notifier/sync_notifier_observer.h"

namespace syncer {

// A helper class for classes that want to implement the SyncNotifier interface.
// It helps keep track of registered handlers and which object ID registrations
// are associated with which handlers, so implementors can just reuse the logic
// here to dispatch invalidations and other interesting notifications.
class SyncNotifierHelper {
 public:
  SyncNotifierHelper();
  ~SyncNotifierHelper();

  // Updates the set of ObjectIds associated with a given |handler|. Passing an
  // empty ObjectIdSet will unregister |handler|. The return value is an
  // ObjectIdSet containing values for all existing handlers.
  ObjectIdSet UpdateRegisteredIds(SyncNotifierObserver* handler,
                                  const ObjectIdSet& ids);

  // Helper that sorts incoming invalidations into a bucket for each handler
  // and then dispatches the batched invalidations to the corresponding handler.
  void DispatchInvalidationsToHandlers(const ObjectIdPayloadMap& id_payloads,
                                       IncomingNotificationSource source);

  // Calls the given handler method for each handler that has registered IDs.
  void EmitOnNotificationsEnabled();
  void EmitOnNotificationsDisabled(NotificationsDisabledReason reason);

 private:
  typedef std::map<invalidation::ObjectId,
                   SyncNotifierObserver*,
                   ObjectIdLessThan> ObjectIdHandlerMap;

  ObserverList<SyncNotifierObserver> handlers_;
  ObjectIdHandlerMap id_to_handler_map_;

  DISALLOW_COPY_AND_ASSIGN(SyncNotifierHelper);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_SYNC_NOTIFIER_HELPER_H_
