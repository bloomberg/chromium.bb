// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/sync_invalidation_listener.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/tracked_objects.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/registration_manager.h"

namespace {

const char kApplicationName[] = "chrome-sync";

}  // namespace

namespace syncer {

SyncInvalidationListener::Delegate::~Delegate() {}

SyncInvalidationListener::SyncInvalidationListener(
    scoped_ptr<notifier::PushClient> push_client)
    : push_client_(push_client.get()),
      sync_system_resources_(push_client.Pass(),
                             ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      delegate_(NULL),
      ticl_state_(DEFAULT_INVALIDATION_ERROR),
      push_client_state_(DEFAULT_INVALIDATION_ERROR) {
  DCHECK(CalledOnValidThread());
  push_client_->AddObserver(this);
}

SyncInvalidationListener::~SyncInvalidationListener() {
  DCHECK(CalledOnValidThread());
  push_client_->RemoveObserver(this);
  Stop();
  DCHECK(!delegate_);
}

void SyncInvalidationListener::Start(
    const CreateInvalidationClientCallback&
        create_invalidation_client_callback,
    const std::string& client_id, const std::string& client_info,
    const std::string& state,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const WeakHandle<InvalidationStateTracker>& invalidation_state_tracker,
    Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  Stop();

  sync_system_resources_.set_platform(client_info);
  sync_system_resources_.Start();

  // The Storage resource is implemented as a write-through cache.  We populate
  // it with the initial state on startup, so subsequent writes go to disk and
  // update the in-memory cache, while reads just return the cached state.
  sync_system_resources_.storage()->SetInitialState(state);

  max_invalidation_versions_ = initial_max_invalidation_versions;
  if (max_invalidation_versions_.empty()) {
    DVLOG(2) << "No initial max invalidation versions for any id";
  } else {
    for (InvalidationVersionMap::const_iterator it =
             max_invalidation_versions_.begin();
         it != max_invalidation_versions_.end(); ++it) {
      DVLOG(2) << "Initial max invalidation version for "
               << ObjectIdToString(it->first) << " is "
               << it->second;
    }
  }
  invalidation_state_tracker_ = invalidation_state_tracker;
  DCHECK(invalidation_state_tracker_.IsInitialized());

  DCHECK(!delegate_);
  DCHECK(delegate);
  delegate_ = delegate;

  int client_type = ipc::invalidation::ClientType::CHROME_SYNC;
  invalidation_client_.reset(
      create_invalidation_client_callback.Run(
          &sync_system_resources_, client_type, client_id,
          kApplicationName, this));
  invalidation_client_->Start();

  registration_manager_.reset(
      new RegistrationManager(invalidation_client_.get()));
}

void SyncInvalidationListener::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(CalledOnValidThread());
  sync_system_resources_.network()->UpdateCredentials(email, token);
}

void SyncInvalidationListener::UpdateRegisteredIds(const ObjectIdSet& ids) {
  DCHECK(CalledOnValidThread());
  registered_ids_ = ids;
  // |ticl_state_| can go to INVALIDATIONS_ENABLED even without a
  // working XMPP connection (as observed by us), so check it instead
  // of GetState() (see http://crbug.com/139424).
  if (ticl_state_ == INVALIDATIONS_ENABLED && registration_manager_.get()) {
    DoRegistrationUpdate();
  }
}

void SyncInvalidationListener::Ready(
    invalidation::InvalidationClient* client) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  ticl_state_ = INVALIDATIONS_ENABLED;
  EmitStateChange();
  DoRegistrationUpdate();
}

void SyncInvalidationListener::Invalidate(
    invalidation::InvalidationClient* client,
    const invalidation::Invalidation& invalidation,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "Invalidate: " << InvalidationToString(invalidation);

  const invalidation::ObjectId& id = invalidation.object_id();

  // The invalidation API spec allows for the possibility of redundant
  // invalidations, so keep track of the max versions and drop
  // invalidations with old versions.
  //
  // TODO(akalin): Now that we keep track of registered ids, we
  // should drop invalidations for unregistered ids.  We may also
  // have to filter it at a higher level, as invalidations for
  // newly-unregistered ids may already be in flight.
  InvalidationVersionMap::const_iterator it =
      max_invalidation_versions_.find(id);
  if ((it != max_invalidation_versions_.end()) &&
      (invalidation.version() <= it->second)) {
    // Drop redundant invalidations.
    client->Acknowledge(ack_handle);
    return;
  }
  DVLOG(2) << "Setting max invalidation version for " << ObjectIdToString(id)
           << " to " << invalidation.version();
  max_invalidation_versions_[id] = invalidation.version();
  invalidation_state_tracker_.Call(
      FROM_HERE,
      &InvalidationStateTracker::SetMaxVersion,
      id, invalidation.version());

  std::string payload;
  // payload() CHECK()'s has_payload(), so we must check it ourselves first.
  if (invalidation.has_payload())
    payload = invalidation.payload();

  ObjectIdStateMap id_state_map;
  id_state_map[id].payload = payload;
  EmitInvalidation(id_state_map);
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  client->Acknowledge(ack_handle);
}

void SyncInvalidationListener::InvalidateUnknownVersion(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InvalidateUnknownVersion";

  ObjectIdStateMap id_state_map;
  id_state_map[object_id].payload = std::string();
  EmitInvalidation(id_state_map);
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  client->Acknowledge(ack_handle);
}

// This should behave as if we got an invalidation with version
// UNKNOWN_OBJECT_VERSION for all known data types.
void SyncInvalidationListener::InvalidateAll(
    invalidation::InvalidationClient* client,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InvalidateAll";

  ObjectIdStateMap id_state_map;
  for (ObjectIdSet::const_iterator it = registered_ids_.begin();
       it != registered_ids_.end(); ++it) {
    id_state_map[*it].payload = std::string();
  }
  EmitInvalidation(id_state_map);
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/76482).
  client->Acknowledge(ack_handle);
}

void SyncInvalidationListener::EmitInvalidation(
    const ObjectIdStateMap& id_state_map) {
  DCHECK(CalledOnValidThread());
  delegate_->OnInvalidate(id_state_map);
}

void SyncInvalidationListener::InformRegistrationStatus(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      InvalidationListener::RegistrationState new_state) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InformRegistrationStatus: "
           << ObjectIdToString(object_id) << " " << new_state;

  if (new_state != InvalidationListener::REGISTERED) {
    // Let |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(object_id);
  }
}

void SyncInvalidationListener::InformRegistrationFailure(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    bool is_transient,
    const std::string& error_message) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InformRegistrationFailure: "
           << ObjectIdToString(object_id)
           << "is_transient=" << is_transient
           << ", message=" << error_message;

  if (is_transient) {
    // We don't care about |unknown_hint|; we let
    // |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(object_id);
  } else {
    // Non-transient failures require an action to resolve. This could happen
    // because:
    // - the server doesn't yet recognize the data type, which could happen for
    //   brand-new data types.
    // - the user has changed his password and hasn't updated it yet locally.
    // Either way, block future registration attempts for |object_id|. However,
    // we don't forget any saved invalidation state since we may use it once the
    // error is addressed.
    registration_manager_->DisableId(object_id);
  }
}

void SyncInvalidationListener::ReissueRegistrations(
    invalidation::InvalidationClient* client,
    const std::string& prefix,
    int prefix_length) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "AllRegistrationsLost";
  registration_manager_->MarkAllRegistrationsLost();
}

void SyncInvalidationListener::InformError(
    invalidation::InvalidationClient* client,
    const invalidation::ErrorInfo& error_info) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(client, invalidation_client_.get());
  LOG(ERROR) << "Ticl error " << error_info.error_reason() << ": "
             << error_info.error_message()
             << " (transient = " << error_info.is_transient() << ")";
  if (error_info.error_reason() == invalidation::ErrorReason::AUTH_FAILURE) {
    ticl_state_ = INVALIDATION_CREDENTIALS_REJECTED;
  } else {
    ticl_state_ = TRANSIENT_INVALIDATION_ERROR;
  }
  EmitStateChange();
}

void SyncInvalidationListener::WriteState(const std::string& state) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "WriteState";
  invalidation_state_tracker_.Call(
      FROM_HERE, &InvalidationStateTracker::SetInvalidationState, state);
}

void SyncInvalidationListener::DoRegistrationUpdate() {
  DCHECK(CalledOnValidThread());
  const ObjectIdSet& unregistered_ids =
      registration_manager_->UpdateRegisteredIds(registered_ids_);
  invalidation_state_tracker_.Call(
      FROM_HERE, &InvalidationStateTracker::Forget, unregistered_ids);
}

void SyncInvalidationListener::StopForTest() {
  DCHECK(CalledOnValidThread());
  Stop();
}

void SyncInvalidationListener::Stop() {
  DCHECK(CalledOnValidThread());
  if (!invalidation_client_.get()) {
    return;
  }

  registration_manager_.reset();
  sync_system_resources_.Stop();
  invalidation_client_->Stop();

  invalidation_client_.reset();
  delegate_ = NULL;

  invalidation_state_tracker_.Reset();
  max_invalidation_versions_.clear();
  ticl_state_ = DEFAULT_INVALIDATION_ERROR;
  push_client_state_ = DEFAULT_INVALIDATION_ERROR;
}

InvalidatorState SyncInvalidationListener::GetState() const {
  DCHECK(CalledOnValidThread());
  if (ticl_state_ == INVALIDATION_CREDENTIALS_REJECTED ||
      push_client_state_ == INVALIDATION_CREDENTIALS_REJECTED) {
    // If either the ticl or the push client rejected our credentials,
    // return INVALIDATION_CREDENTIALS_REJECTED.
    return INVALIDATION_CREDENTIALS_REJECTED;
  }
  if (ticl_state_ == INVALIDATIONS_ENABLED &&
      push_client_state_ == INVALIDATIONS_ENABLED) {
    // If the ticl is ready and the push client notifications are
    // enabled, return INVALIDATIONS_ENABLED.
    return INVALIDATIONS_ENABLED;
  }
  // Otherwise, we have a transient error.
  return TRANSIENT_INVALIDATION_ERROR;
}

void SyncInvalidationListener::EmitStateChange() {
  DCHECK(CalledOnValidThread());
  delegate_->OnInvalidatorStateChange(GetState());
}

void SyncInvalidationListener::OnNotificationsEnabled() {
  DCHECK(CalledOnValidThread());
  push_client_state_ = INVALIDATIONS_ENABLED;
  EmitStateChange();
}

void SyncInvalidationListener::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  DCHECK(CalledOnValidThread());
  push_client_state_ = FromNotifierReason(reason);
  EmitStateChange();
}

void SyncInvalidationListener::OnIncomingNotification(
    const notifier::Notification& notification) {
  DCHECK(CalledOnValidThread());
  // Do nothing, since this is already handled by |invalidation_client_|.
}

}  // namespace syncer
