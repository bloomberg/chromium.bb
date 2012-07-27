// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/sync_notifier_helper.h"

#include "base/logging.h"

namespace syncer {

SyncNotifierHelper::SyncNotifierHelper() {}
SyncNotifierHelper::~SyncNotifierHelper() {}

ObjectIdSet SyncNotifierHelper::UpdateRegisteredIds(
    SyncNotifierObserver* handler, const ObjectIdSet& ids) {
  if (ids.empty()) {
    handlers_.RemoveObserver(handler);
  } else if (!handlers_.HasObserver(handler)) {
    handlers_.AddObserver(handler);
  }

  ObjectIdSet registered_ids(ids);
  // Remove all existing entries for |handler| and fill |registered_ids| with
  // the rest.
  for (ObjectIdHandlerMap::iterator it = id_to_handler_map_.begin();
       it != id_to_handler_map_.end(); ) {
    if (it->second == handler) {
      ObjectIdHandlerMap::iterator erase_it = it;
      ++it;
      id_to_handler_map_.erase(erase_it);
    } else {
      registered_ids.insert(it->first);
      ++it;
    }
  }

  // Now add the entries for |handler|. We keep track of the last insertion
  // point so we only traverse the map once to insert all the new entries.
  ObjectIdHandlerMap::iterator insert_it = id_to_handler_map_.begin();
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    insert_it = id_to_handler_map_.insert(insert_it,
                                          std::make_pair(*it, handler));
    CHECK_EQ(handler, insert_it->second) << "Duplicate registration for "
                                         << ObjectIdToString(insert_it->first);
  }
  if (logging::DEBUG_MODE) {
    // The mapping shouldn't contain any handlers that aren't in |handlers_|.
    for (ObjectIdHandlerMap::const_iterator it = id_to_handler_map_.begin();
         it != id_to_handler_map_.end(); ++it) {
      CHECK(handlers_.HasObserver(it->second));
    }
  }
  return registered_ids;
}

void SyncNotifierHelper::DispatchInvalidationsToHandlers(
    const ObjectIdPayloadMap& id_payloads,
    IncomingNotificationSource source) {
  typedef std::map<SyncNotifierObserver*, ObjectIdPayloadMap> DispatchMap;
  DispatchMap dispatch_map;
  for (ObjectIdPayloadMap::const_iterator it = id_payloads.begin();
       it != id_payloads.end(); ++it) {
    ObjectIdHandlerMap::const_iterator find_it =
        id_to_handler_map_.find(it->first);
    // If we get an invalidation with a source type that we can't map to an
    // observer, just drop it--the observer was unregistered while the
    // invalidation was in flight.
    if (find_it == id_to_handler_map_.end())
      continue;
    dispatch_map[find_it->second].insert(*it);
  }

  if (handlers_.might_have_observers()) {
    ObserverListBase<SyncNotifierObserver>::Iterator it(handlers_);
    SyncNotifierObserver* handler = NULL;
    while ((handler = it.GetNext()) != NULL) {
      DispatchMap::const_iterator dispatch_it = dispatch_map.find(handler);
      if (dispatch_it != dispatch_map.end()) {
        handler->OnIncomingNotification(dispatch_it->second, source);
      }
    }
  }
}

void SyncNotifierHelper::EmitOnNotificationsEnabled() {
  FOR_EACH_OBSERVER(SyncNotifierObserver, handlers_, OnNotificationsEnabled());
}

void SyncNotifierHelper::EmitOnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  FOR_EACH_OBSERVER(SyncNotifierObserver, handlers_,
                    OnNotificationsDisabled(reason));
}

}  // namespace syncer
