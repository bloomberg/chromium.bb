// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_SERVICE_H_

#include "base/callback_forward.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace syncer {
class InvalidationHandler;
}  // namespace syncer

namespace invalidation {
class InvalidationLogger;

// Interface for classes that handle invalidation subscriptions and send out
// invalidations to registered handlers.
//
// Invalidation clients should follow the pattern below:
//
// When starting the client:
//
//   service->RegisterInvalidationHandler(client_handler);
//
// When the set of topics to register changes for the client during its
// lifetime (i.e., between calls to RegisterInvalidationHandler(client_handler)
// and UnregisterInvalidationHandler(client_handler):
//
//   service->UpdateInterestedTopics(client_handler, client_topics);
//
// When shutting down the client for browser shutdown:
//
//   service->UnregisterInvalidationHandler(client_handler);
//
// Note that there's no call to UpdateInterestedTopics() -- this is because the
// invalidation API persists interested topics across browser restarts.
//
// When permanently shutting down the client, e.g. when disabling the related
// feature:
//
//   service->UpdateInterestedTopics(client_handler, {});
//   service->UnregisterInvalidationHandler(client_handler);
//
// If an invalidation handler cares about the invalidator state, it should also
// do the following when starting the client:
//
//   invalidator_state = service->GetInvalidatorState();
//
// It can also do the above in OnInvalidatorStateChange(), or it can use the
// argument to OnInvalidatorStateChange().
//
// It is an error to have registered handlers when an
// InvalidationService is shut down; clients must ensure that they
// unregister themselves before then. (Depending on the
// InvalidationService, shutdown may be equivalent to destruction, or
// a separate function call like Shutdown()).
//
// NOTE(akalin): Invalidations that come in during browser shutdown may get
// dropped.  This won't matter once we have an Acknowledge API, though: see
// http://crbug.com/78462 and http://crbug.com/124149.
class InvalidationService {
 public:
  virtual ~InvalidationService() {}

  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must not already be registered.
  //
  // Handler registrations are persisted across restarts of sync.
  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) = 0;

  // Updates the set of topics associated with |handler|. |handler| must not be
  // nullptr, and must already be registered. A topic must be subscribed for at
  // most one handler. If topic is already subscribed for another
  // InvalidationHandler function returns false.
  //
  // Subscribed topics are persisted across restarts of sync.
  virtual bool UpdateInterestedTopics(syncer::InvalidationHandler* handler,
                                      const syncer::TopicSet& topics)
      WARN_UNUSED_RESULT = 0;

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  //
  // Handler registrations are persisted across restarts of sync.
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) = 0;

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this must return
  // the updated state.
  virtual syncer::InvalidatorState GetInvalidatorState() const = 0;

  // Returns the ID belonging to this invalidation client.  Can be used to
  // prevent the receipt of notifications of our own changes.
  virtual std::string GetInvalidatorClientId() const = 0;

  // Return the logger used to debug invalidations
  virtual InvalidationLogger* GetInvalidationLogger() = 0;

  // Triggers requests of internal status.
  virtual void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> post_caller)
      const = 0;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_SERVICE_H_
