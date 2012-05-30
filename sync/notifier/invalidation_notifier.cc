// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_notifier.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "jingle/notifier/listener/push_client.h"
#include "net/url_request/url_request_context.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "sync/syncable/model_type_payload_map.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace sync_notifier {

InvalidationNotifier::InvalidationNotifier(
    scoped_ptr<notifier::PushClient> push_client,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const browser_sync::WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    const std::string& client_info)
    : state_(STOPPED),
      initial_max_invalidation_versions_(initial_max_invalidation_versions),
      invalidation_state_tracker_(invalidation_state_tracker),
      client_info_(client_info),
      invalidation_client_(push_client.Pass()) {
}

InvalidationNotifier::~InvalidationNotifier() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void InvalidationNotifier::AddObserver(SyncNotifierObserver* observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void InvalidationNotifier::RemoveObserver(SyncNotifierObserver* observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void InvalidationNotifier::SetUniqueId(const std::string& unique_id) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation_client_id_ = unique_id;
  DVLOG(1) << "Setting unique ID to " << unique_id;
  CHECK(!invalidation_client_id_.empty());
}

void InvalidationNotifier::SetState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation_state_ = state;
  DVLOG(1) << "Setting new state";
}

void InvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  if (state_ == STOPPED) {
    invalidation_client_.Start(
        invalidation_client_id_, client_info_, invalidation_state_,
        initial_max_invalidation_versions_,
        invalidation_state_tracker_,
        this, this);
    invalidation_state_.clear();
    state_ = STARTED;
  }
  invalidation_client_.UpdateCredentials(email, token);
}

void InvalidationNotifier::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(!invalidation_client_id_.empty());
  invalidation_client_.RegisterTypes(enabled_types);
}

void InvalidationNotifier::SendNotification(
    syncable::ModelTypeSet changed_types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  // Do nothing.
}

void InvalidationNotifier::OnInvalidate(
    const syncable::ModelTypePayloadMap& type_payloads) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnIncomingNotification(type_payloads,
                                           sync_notifier::REMOTE_NOTIFICATION));
}

void InvalidationNotifier::OnSessionStatusChanged(bool has_session) {
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationStateChange(has_session));
}

void InvalidationNotifier::WriteState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "WriteState";
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_, StoreState(state));
}

}  // namespace sync_notifier
