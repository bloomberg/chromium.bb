// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_notifier.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "google/cacheinvalidation/include/invalidation-client-factory.h"
#include "jingle/notifier/listener/push_client.h"
#include "net/url_request/url_request_context.h"
#include "sync/notifier/invalidation_handler.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace syncer {

InvalidationNotifier::InvalidationNotifier(
    scoped_ptr<notifier::PushClient> push_client,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const std::string& initial_invalidation_state,
    const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
    const std::string& client_info)
    : state_(STOPPED),
      initial_max_invalidation_versions_(initial_max_invalidation_versions),
      invalidation_state_tracker_(invalidation_state_tracker),
      client_info_(client_info),
      invalidation_state_(initial_invalidation_state),
      invalidation_listener_(push_client.Pass()) {
}

InvalidationNotifier::~InvalidationNotifier() {
  DCHECK(CalledOnValidThread());
}

void InvalidationNotifier::RegisterHandler(InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  registrar_.RegisterHandler(handler);
}

void InvalidationNotifier::UpdateRegisteredIds(InvalidationHandler* handler,
                                               const ObjectIdSet& ids) {
  DCHECK(CalledOnValidThread());
  registrar_.UpdateRegisteredIds(handler, ids);
  invalidation_listener_.UpdateRegisteredIds(registrar_.GetAllRegisteredIds());
}

void InvalidationNotifier::UnregisterHandler(InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  registrar_.UnregisterHandler(handler);
}

InvalidatorState InvalidationNotifier::GetInvalidatorState() const {
  DCHECK(CalledOnValidThread());
  return registrar_.GetInvalidatorState();
}

void InvalidationNotifier::SetUniqueId(const std::string& unique_id) {
  DCHECK(CalledOnValidThread());
  client_id_ = unique_id;
  DVLOG(1) << "Setting unique ID to " << unique_id;
  CHECK(!client_id_.empty());
}

void InvalidationNotifier::SetStateDeprecated(const std::string& state) {
  DCHECK(CalledOnValidThread());
  DCHECK_LT(state_, STARTED);
  if (invalidation_state_.empty()) {
    // Migrate state from sync to invalidation state tracker (bug
    // 124140).  We've just been handed state from the syncable::Directory, and
    // the initial invalidation state was empty, implying we've never written
    // to the new store. Do this here to ensure we always migrate (even if
    // we fail to establish an initial connection or receive an initial
    // invalidation) so that we can make the old code obsolete as soon as
    // possible.
    invalidation_state_ = state;
    invalidation_state_tracker_.Call(
        FROM_HERE, &InvalidationStateTracker::SetInvalidationState, state);
    UMA_HISTOGRAM_BOOLEAN("InvalidationNotifier.UsefulSetState", true);
  } else {
    UMA_HISTOGRAM_BOOLEAN("InvalidationNotifier.UsefulSetState", false);
  }
}

void InvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  if (state_ == STOPPED) {
    invalidation_listener_.Start(
        base::Bind(&invalidation::CreateInvalidationClient),
        client_id_, client_info_, invalidation_state_,
        initial_max_invalidation_versions_,
        invalidation_state_tracker_,
        this);
    invalidation_state_.clear();
    state_ = STARTED;
  }
  invalidation_listener_.UpdateCredentials(email, token);
}

void InvalidationNotifier::SendInvalidation(
    const ObjectIdStateMap& id_state_map) {
  DCHECK(CalledOnValidThread());
  // Do nothing.
}

void InvalidationNotifier::OnInvalidate(const ObjectIdStateMap& id_state_map) {
  DCHECK(CalledOnValidThread());
  registrar_.DispatchInvalidationsToHandlers(id_state_map, REMOTE_INVALIDATION);
}

void InvalidationNotifier::OnInvalidatorStateChange(InvalidatorState state) {
  DCHECK(CalledOnValidThread());
  registrar_.UpdateInvalidatorState(state);
}

}  // namespace syncer
