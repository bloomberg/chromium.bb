// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface to the invalidator, which is an object that receives
// invalidations for registered object IDs. The corresponding
// InvalidationHandler is notifier when such an event occurs.

#ifndef SYNC_NOTIFIER_INVALIDATOR_H_
#define SYNC_NOTIFIER_INVALIDATOR_H_

#include <string>

#include "sync/internal_api/public/base/model_type.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/object_id_state_map.h"

namespace syncer {
class InvalidationHandler;

class Invalidator {
 public:
  Invalidator() {}
  virtual ~Invalidator() {}

  // Clients should follow the pattern below:
  //
  // When starting the client:
  //
  //   invalidator->RegisterHandler(client_handler);
  //
  // When the set of IDs to register changes for the client during its lifetime
  // (i.e., between calls to RegisterHandler(client_handler) and
  // UnregisterHandler(client_handler):
  //
  //   invalidator->UpdateRegisteredIds(client_handler, client_ids);
  //
  // When shutting down the client for profile shutdown:
  //
  //   invalidator->UnregisterHandler(client_handler);
  //
  // Note that there's no call to UpdateRegisteredIds() -- this is because the
  // invalidation API persists registrations across browser restarts.
  //
  // When permanently shutting down the client, e.g. when disabling the related
  // feature:
  //
  //   invalidator->UpdateRegisteredIds(client_handler, ObjectIdSet());
  //   invalidator->UnregisterHandler(client_handler);

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must not already be registered.
  virtual void RegisterHandler(InvalidationHandler* handler) = 0;

  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler.
  virtual void UpdateRegisteredIds(InvalidationHandler* handler,
                                   const ObjectIdSet& ids) = 0;

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  virtual void UnregisterHandler(InvalidationHandler* handler) = 0;

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this must return
  // the updated state.
  virtual InvalidatorState GetInvalidatorState() const = 0;

  // SetUniqueId must be called once, before any call to
  // UpdateCredentials.  |unique_id| should be a non-empty globally
  // unique string.
  virtual void SetUniqueId(const std::string& unique_id) = 0;

  // SetState must be called once, before any call to
  // UpdateCredentials.  |state| may be empty.
  // Deprecated in favour of InvalidationStateTracker persistence.
  virtual void SetStateDeprecated(const std::string& state) = 0;

  // The observers won't be notified of any notifications until
  // UpdateCredentials is called at least once. It can be called more than
  // once.
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) = 0;

  // This is here only to support the old p2p notification implementation,
  // which is still used by sync integration tests.
  // TODO(akalin): Remove this once we move the integration tests off p2p
  // notifications.
  virtual void SendInvalidation(const ObjectIdStateMap& id_state_map) = 0;
};
}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATOR_H_
