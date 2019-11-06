// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_H_

#include <map>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_util.h"

namespace syncer {

class TopicInvalidationMap;

// A helper class for implementations of the Invalidator interface.  It helps
// keep track of registered handlers and which object ID registrations are
// associated with which handlers, so implementors can just reuse the logic
// here to dispatch invalidations and other interesting notifications.
class INVALIDATION_EXPORT InvalidatorRegistrar {
 public:
  InvalidatorRegistrar();

  // It is an error to have registered handlers on destruction.
  ~InvalidatorRegistrar();

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must already be registered.
  void RegisterHandler(InvalidationHandler* handler);

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  void UnregisterHandler(InvalidationHandler* handler);

  // Updates the set of topics associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  A topic must be registered
  // for at most one handler. If topic is already registered function returns
  // false.
  virtual bool UpdateRegisteredTopics(InvalidationHandler* handler,
                                      const Topics& topics) WARN_UNUSED_RESULT;

  virtual Topics GetRegisteredTopics(InvalidationHandler* handler) const;

  // Returns the set of all IDs that are registered to some handler (even
  // handlers that have been unregistered).
  virtual Topics GetAllRegisteredIds() const;

  // Sorts incoming invalidations into a bucket for each handler and then
  // dispatches the batched invalidations to the corresponding handler.
  // Invalidations for IDs with no corresponding handler are dropped, as are
  // invalidations for handlers that are not added.
  void DispatchInvalidationsToHandlers(
      const TopicInvalidationMap& invalidation_map);

  // Updates the invalidator state to the given one and then notifies
  // all handlers.  Note that the order is important; handlers that
  // call GetInvalidatorState() when notified will see the new state.
  void UpdateInvalidatorState(InvalidatorState state);

  // Updates the invalidator id to the given one and then notifies
  // all handlers.
  void UpdateInvalidatorId(const std::string& id);

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this returns the
  // updated state.
  InvalidatorState GetInvalidatorState() const;

  // Gets a new map for the name of invalidator handlers and their
  // objects id. This is used by the InvalidatorLogger to be able
  // to display every registered handlers and its objectsIds.
  std::map<std::string, Topics> GetSanitizedHandlersIdsMap();

  bool IsHandlerRegistered(const InvalidationHandler* handler) const;
  bool HasRegisteredHandlers() const;

  // Needed for death tests.
  void DetachFromThreadForTest();

 private:
  typedef std::map<InvalidationHandler*, Topics> HandlerTopicsMap;

  base::ThreadChecker thread_checker_;
  base::ObserverList<InvalidationHandler, true>::Unchecked handlers_;
  HandlerTopicsMap handler_to_topics_map_;
  InvalidatorState state_;

  DISALLOW_COPY_AND_ASSIGN(InvalidatorRegistrar);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATOR_REGISTRAR_H_
