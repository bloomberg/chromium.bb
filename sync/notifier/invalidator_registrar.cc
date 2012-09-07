// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidator_registrar.h"

#include <cstddef>
#include <utility>

#include "base/logging.h"

namespace syncer {

InvalidatorRegistrar::InvalidatorRegistrar()
    : state_(DEFAULT_INVALIDATION_ERROR) {}

InvalidatorRegistrar::~InvalidatorRegistrar() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void InvalidatorRegistrar::RegisterHandler(InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(!handlers_.HasObserver(handler));
  handlers_.AddObserver(handler);
}

void InvalidatorRegistrar::UpdateRegisteredIds(
    InvalidationHandler* handler,
    const ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));
  // Remove all existing entries for |handler|.
  for (IdHandlerMap::iterator it = id_to_handler_map_.begin();
       it != id_to_handler_map_.end(); ) {
    if (it->second == handler) {
      IdHandlerMap::iterator erase_it = it;
      ++it;
      id_to_handler_map_.erase(erase_it);
    } else {
      ++it;
    }
  }

  // Now add the entries for |handler|. We keep track of the last insertion
  // point so we only traverse the map once to insert all the new entries.
  IdHandlerMap::iterator insert_it = id_to_handler_map_.begin();
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    insert_it =
        id_to_handler_map_.insert(insert_it, std::make_pair(*it, handler));
    CHECK_EQ(handler, insert_it->second)
        << "Duplicate registration: trying to register "
        << ObjectIdToString(insert_it->first) << " for "
        << handler << " when it's already registered for "
        << insert_it->second;
  }
}

void InvalidatorRegistrar::UnregisterHandler(InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(handler);
  CHECK(handlers_.HasObserver(handler));
  handlers_.RemoveObserver(handler);
}

ObjectIdSet InvalidatorRegistrar::GetRegisteredIds(
    InvalidationHandler* handler) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ObjectIdSet registered_ids;
  for (IdHandlerMap::const_iterator it = id_to_handler_map_.begin();
       it != id_to_handler_map_.end(); ++it) {
    if (it->second == handler) {
      registered_ids.insert(it->first);
    }
  }
  return registered_ids;
}

ObjectIdSet InvalidatorRegistrar::GetAllRegisteredIds() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ObjectIdSet registered_ids;
  for (IdHandlerMap::const_iterator it = id_to_handler_map_.begin();
       it != id_to_handler_map_.end(); ++it) {
    registered_ids.insert(it->first);
  }
  return registered_ids;
}

void InvalidatorRegistrar::DispatchInvalidationsToHandlers(
    const ObjectIdStateMap& id_state_map,
    IncomingInvalidationSource source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If we have no handlers, there's nothing to do.
  if (!handlers_.might_have_observers()) {
    return;
  }

  typedef std::map<InvalidationHandler*, ObjectIdStateMap> DispatchMap;
  DispatchMap dispatch_map;
  for (ObjectIdStateMap::const_iterator it = id_state_map.begin();
       it != id_state_map.end(); ++it) {
    InvalidationHandler* const handler = ObjectIdToHandler(it->first);
    // Filter out invalidations for IDs with no handler.
    if (handler)
      dispatch_map[handler].insert(*it);
  }

  // Emit invalidations only for handlers in |handlers_|.
  ObserverListBase<InvalidationHandler>::Iterator it(handlers_);
  InvalidationHandler* handler = NULL;
  while ((handler = it.GetNext()) != NULL) {
    DispatchMap::const_iterator dispatch_it = dispatch_map.find(handler);
    if (dispatch_it != dispatch_map.end())
      handler->OnIncomingInvalidation(dispatch_it->second, source);
  }
}

void InvalidatorRegistrar::UpdateInvalidatorState(InvalidatorState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_ = state;
  FOR_EACH_OBSERVER(InvalidationHandler, handlers_,
                    OnInvalidatorStateChange(state));
}

InvalidatorState InvalidatorRegistrar::GetInvalidatorState() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return state_;
}

bool InvalidatorRegistrar::IsHandlerRegisteredForTest(
    InvalidationHandler* handler) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return handlers_.HasObserver(handler);
}

void InvalidatorRegistrar::DetachFromThreadForTest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  thread_checker_.DetachFromThread();
}

InvalidationHandler* InvalidatorRegistrar::ObjectIdToHandler(
    const invalidation::ObjectId& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  IdHandlerMap::const_iterator it = id_to_handler_map_.find(id);
  return (it == id_to_handler_map_.end()) ? NULL : it->second;
}

}  // namespace syncer
