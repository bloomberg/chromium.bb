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
    const std::string& invalidator_client_id,
    const InvalidationStateMap& initial_invalidation_state_map,
    const std::string& invalidation_bootstrap_data,
    const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
    const std::string& client_info)
    : state_(STOPPED),
      initial_invalidation_state_map_(initial_invalidation_state_map),
      invalidation_state_tracker_(invalidation_state_tracker),
      client_info_(client_info),
      invalidator_client_id_(invalidator_client_id),
      invalidation_bootstrap_data_(invalidation_bootstrap_data),
      invalidation_listener_(&tick_clock_, push_client.Pass()) {
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

void InvalidationNotifier::Acknowledge(const invalidation::ObjectId& id,
                                       const AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  invalidation_listener_.Acknowledge(id, ack_handle);
}

InvalidatorState InvalidationNotifier::GetInvalidatorState() const {
  DCHECK(CalledOnValidThread());
  return registrar_.GetInvalidatorState();
}

void InvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  if (state_ == STOPPED) {
    invalidation_listener_.Start(
        base::Bind(&invalidation::CreateInvalidationClient),
        invalidator_client_id_, client_info_, invalidation_bootstrap_data_,
        initial_invalidation_state_map_,
        invalidation_state_tracker_,
        this);
    state_ = STARTED;
  }
  invalidation_listener_.UpdateCredentials(email, token);
}

void InvalidationNotifier::SendInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(CalledOnValidThread());
  // Do nothing.
}

void InvalidationNotifier::OnInvalidate(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(CalledOnValidThread());
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

void InvalidationNotifier::OnInvalidatorStateChange(InvalidatorState state) {
  DCHECK(CalledOnValidThread());
  registrar_.UpdateInvalidatorState(state);
}

}  // namespace syncer
