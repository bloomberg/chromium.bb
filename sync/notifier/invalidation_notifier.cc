// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_notifier.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "jingle/notifier/listener/push_client.h"
#include "net/url_request/url_request_context.h"
#include "sync/internal_api/public/base/model_type_payload_map.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace syncer {

InvalidationNotifier::InvalidationNotifier(
    scoped_ptr<notifier::PushClient> push_client,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const std::string& initial_invalidation_state,
    const syncer::WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    const std::string& client_info)
    : state_(STOPPED),
      initial_max_invalidation_versions_(initial_max_invalidation_versions),
      invalidation_state_tracker_(invalidation_state_tracker),
      client_info_(client_info),
      invalidation_state_(initial_invalidation_state),
      invalidation_client_(push_client.Pass()) {
}

InvalidationNotifier::~InvalidationNotifier() {
  DCHECK(CalledOnValidThread());
}

void InvalidationNotifier::AddObserver(SyncNotifierObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void InvalidationNotifier::RemoveObserver(SyncNotifierObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void InvalidationNotifier::SetUniqueId(const std::string& unique_id) {
  DCHECK(CalledOnValidThread());
  invalidation_client_id_ = unique_id;
  DVLOG(1) << "Setting unique ID to " << unique_id;
  CHECK(!invalidation_client_id_.empty());
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
    invalidation_client_.Start(
        invalidation_client_id_, client_info_, invalidation_state_,
        initial_max_invalidation_versions_,
        invalidation_state_tracker_,
        this);
    invalidation_state_.clear();
    state_ = STARTED;
  }
  invalidation_client_.UpdateCredentials(email, token);
}

void InvalidationNotifier::UpdateEnabledTypes(
    syncable::ModelTypeSet enabled_types) {
  DCHECK(CalledOnValidThread());
  CHECK(!invalidation_client_id_.empty());
  ObjectIdSet ids;
  for (syncable::ModelTypeSet::Iterator it = enabled_types.First(); it.Good();
       it.Inc()) {
    invalidation::ObjectId id;
    if (!RealModelTypeToObjectId(it.Get(), &id)) {
      DLOG(WARNING) << "Invalid model type " << it.Get();
      continue;
    }
    ids.insert(id);
  }
  invalidation_client_.RegisterIds(ids);
}

void InvalidationNotifier::SendNotification(
    syncable::ModelTypeSet changed_types) {
  DCHECK(CalledOnValidThread());
  // Do nothing.
}

void InvalidationNotifier::OnInvalidate(const ObjectIdPayloadMap& id_payloads) {
  DCHECK(CalledOnValidThread());
  // TODO(dcheng): This should probably be a utility function somewhere...
  syncable::ModelTypePayloadMap type_payloads;
  for (ObjectIdPayloadMap::const_iterator it = id_payloads.begin();
       it != id_payloads.end(); ++it) {
    syncable::ModelType model_type;
    if (!ObjectIdToRealModelType(it->first, &model_type)) {
      DLOG(WARNING) << "Invalid object ID: " << ObjectIdToString(it->first);
      continue;
    }
    type_payloads[model_type] = it->second;
  }
  FOR_EACH_OBSERVER(
      SyncNotifierObserver, observers_,
      OnIncomingNotification(type_payloads,
                             syncer::REMOTE_NOTIFICATION));
}

void InvalidationNotifier::OnNotificationsEnabled() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationsEnabled());
}

void InvalidationNotifier::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationsDisabled(reason));
}

}  // namespace syncer
