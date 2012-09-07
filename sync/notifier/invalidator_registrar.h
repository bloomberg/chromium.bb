// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_INVALIDATOR_REGISTRAR_H_
#define SYNC_NOTIFIER_INVALIDATOR_REGISTRAR_H_

#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/object_id_state_map.h"

namespace invalidation {
class ObjectId;
}  // namespace invalidation

namespace syncer {

// A helper class for implementations of the Invalidator interface.  It helps
// keep track of registered handlers and which object ID registrations are
// associated with which handlers, so implementors can just reuse the logic
// here to dispatch invalidations and other interesting notifications.
class InvalidatorRegistrar {
 public:
  InvalidatorRegistrar();
  ~InvalidatorRegistrar();

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must already be registered.
  void RegisterHandler(InvalidationHandler* handler);

  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler.
  void UpdateRegisteredIds(InvalidationHandler* handler,
                           const ObjectIdSet& ids);

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  void UnregisterHandler(InvalidationHandler* handler);

  ObjectIdSet GetRegisteredIds(InvalidationHandler* handler) const;

  // Returns the set of all IDs that are registered to some handler (even
  // handlers that have been unregistered).
  ObjectIdSet GetAllRegisteredIds() const;

  // Sorts incoming invalidations into a bucket for each handler and then
  // dispatches the batched invalidations to the corresponding handler.
  // Invalidations for IDs with no corresponding handler are dropped, as are
  // invalidations for handlers that are not added.
  void DispatchInvalidationsToHandlers(const ObjectIdStateMap& id_state_map,
                                       IncomingInvalidationSource source);

  // Updates the invalidator state to the given one and then notifies
  // all handlers.  Note that the order is important; handlers that
  // call GetInvalidatorState() when notified will see the new state.
  void UpdateInvalidatorState(InvalidatorState state);

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this returns the
  // updated state.
  InvalidatorState GetInvalidatorState() const;

  bool IsHandlerRegisteredForTest(InvalidationHandler* handler) const;

  // Needed for death tests.
  void DetachFromThreadForTest();

 private:
  typedef std::map<invalidation::ObjectId, InvalidationHandler*,
                   ObjectIdLessThan>
      IdHandlerMap;

  InvalidationHandler* ObjectIdToHandler(const invalidation::ObjectId& id);

  base::ThreadChecker thread_checker_;
  ObserverList<InvalidationHandler> handlers_;
  IdHandlerMap id_to_handler_map_;
  InvalidatorState state_;

  DISALLOW_COPY_AND_ASSIGN(InvalidatorRegistrar);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATOR_REGISTRAR_H_
