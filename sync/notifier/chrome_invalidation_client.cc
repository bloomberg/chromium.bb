// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/chrome_invalidation_client.h"

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/tracked_objects.h"
#include "google/cacheinvalidation/include/invalidation-client-factory.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/v2/types.pb.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/registration_manager.h"

namespace {

const char kApplicationName[] = "chrome-sync";

}  // namespace

namespace csync {

ChromeInvalidationClient::Listener::~Listener() {}

ChromeInvalidationClient::ChromeInvalidationClient(
    scoped_ptr<notifier::PushClient> push_client)
    : push_client_(push_client.get()),
      chrome_system_resources_(push_client.Pass(),
                               ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      listener_(NULL),
      ticl_state_(DEFAULT_NOTIFICATION_ERROR),
      push_client_state_(DEFAULT_NOTIFICATION_ERROR) {
  DCHECK(CalledOnValidThread());
  push_client_->AddObserver(this);
}

ChromeInvalidationClient::~ChromeInvalidationClient() {
  DCHECK(CalledOnValidThread());
  push_client_->RemoveObserver(this);
  Stop();
  DCHECK(!listener_);
}

void ChromeInvalidationClient::Start(
    const std::string& client_id, const std::string& client_info,
    const std::string& state,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const csync::WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    Listener* listener) {
  DCHECK(CalledOnValidThread());
  Stop();

  chrome_system_resources_.set_platform(client_info);
  chrome_system_resources_.Start();

  // The Storage resource is implemented as a write-through cache.  We populate
  // it with the initial state on startup, so subsequent writes go to disk and
  // update the in-memory cache, while reads just return the cached state.
  chrome_system_resources_.storage()->SetInitialState(state);

  max_invalidation_versions_ = initial_max_invalidation_versions;
  if (max_invalidation_versions_.empty()) {
    DVLOG(2) << "No initial max invalidation versions for any type";
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

  DCHECK(!listener_);
  DCHECK(listener);
  listener_ = listener;

  int client_type = ipc::invalidation::ClientType::CHROME_SYNC;
  invalidation_client_.reset(
      invalidation::CreateInvalidationClient(
          &chrome_system_resources_, client_type, client_id,
          kApplicationName, this));
  invalidation_client_->Start();

  registration_manager_.reset(
      new RegistrationManager(invalidation_client_.get()));
}

void ChromeInvalidationClient::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(CalledOnValidThread());
  chrome_system_resources_.network()->UpdateCredentials(email, token);
}

void ChromeInvalidationClient::RegisterTypes(syncable::ModelTypeSet types) {
  DCHECK(CalledOnValidThread());
  // TODO(dcheng): Even though these fields are redundant, we keep both around
  // for convenience while the conversion is still in process.
  registered_types_ = types;
  registered_ids_.clear();
  for (syncable::ModelTypeSet::Iterator it = types.First();
       it.Good(); it.Inc()) {
    invalidation::ObjectId id;
    if (!RealModelTypeToObjectId(it.Get(), &id)) {
      LOG(WARNING) << "Invalid model type: " << it.Get();
      continue;
    }
    registered_ids_.insert(id);
  }
  if (GetState() == NO_NOTIFICATION_ERROR && registration_manager_.get()) {
    registration_manager_->SetRegisteredIds(registered_ids_);
  }
  // TODO(akalin): Clear invalidation versions for unregistered types.
}

void ChromeInvalidationClient::Ready(
    invalidation::InvalidationClient* client) {
  ticl_state_ = NO_NOTIFICATION_ERROR;
  EmitStateChange();
  registration_manager_->SetRegisteredIds(registered_ids_);
}

void ChromeInvalidationClient::Invalidate(
    invalidation::InvalidationClient* client,
    const invalidation::Invalidation& invalidation,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
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

  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(id, &model_type)) {
    LOG(WARNING) << "Could not get invalidation model type; "
                 << "invalidating everything";
    EmitInvalidation(registered_types_, std::string());
    client->Acknowledge(ack_handle);
    return;
  }

  std::string payload;
  // payload() CHECK()'s has_payload(), so we must check it ourselves first.
  if (invalidation.has_payload())
    payload = invalidation.payload();

  EmitInvalidation(syncable::ModelTypeSet(model_type), payload);
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  client->Acknowledge(ack_handle);
}

void ChromeInvalidationClient::InvalidateUnknownVersion(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "InvalidateUnknownVersion";

  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(object_id, &model_type)) {
    LOG(WARNING) << "Could not get invalidation model type; "
                 << "invalidating everything";
    EmitInvalidation(registered_types_, std::string());
    client->Acknowledge(ack_handle);
    return;
  }

  EmitInvalidation(syncable::ModelTypeSet(model_type), "");
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  client->Acknowledge(ack_handle);
}

// This should behave as if we got an invalidation with version
// UNKNOWN_OBJECT_VERSION for all known data types.
void ChromeInvalidationClient::InvalidateAll(
    invalidation::InvalidationClient* client,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "InvalidateAll";
  EmitInvalidation(registered_types_, std::string());
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/76482).
  client->Acknowledge(ack_handle);
}

void ChromeInvalidationClient::EmitInvalidation(
    syncable::ModelTypeSet types, const std::string& payload) {
  DCHECK(CalledOnValidThread());
  syncable::ModelTypePayloadMap type_payloads =
      syncable::ModelTypePayloadMapFromEnumSet(types, payload);
  listener_->OnInvalidate(type_payloads);
}

void ChromeInvalidationClient::InformRegistrationStatus(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      InvalidationListener::RegistrationState new_state) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "InformRegistrationStatus: "
           << ObjectIdToString(object_id) << " " << new_state;

  if (new_state != InvalidationListener::REGISTERED) {
    // Let |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(object_id);
  }
}

void ChromeInvalidationClient::InformRegistrationFailure(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    bool is_transient,
    const std::string& error_message) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "InformRegistrationFailure: "
           << ObjectIdToString(object_id)
           << "is_transient=" << is_transient
           << ", message=" << error_message;

  if (is_transient) {
    // We don't care about |unknown_hint|; we let
    // |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(object_id);
  } else {
    // Non-transient failures are permanent, so block any future
    // registration requests for |model_type|.  (This happens if the
    // server doesn't recognize the data type, which could happen for
    // brand-new data types.)
    registration_manager_->DisableId(object_id);
  }
}

void ChromeInvalidationClient::ReissueRegistrations(
    invalidation::InvalidationClient* client,
    const std::string& prefix,
    int prefix_length) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "AllRegistrationsLost";
  registration_manager_->MarkAllRegistrationsLost();
}

void ChromeInvalidationClient::InformError(
    invalidation::InvalidationClient* client,
    const invalidation::ErrorInfo& error_info) {
  LOG(ERROR) << "Ticl error " << error_info.error_reason() << ": "
             << error_info.error_message()
             << " (transient = " << error_info.is_transient() << ")";
  if (error_info.error_reason() == invalidation::ErrorReason::AUTH_FAILURE) {
    ticl_state_ = NOTIFICATION_CREDENTIALS_REJECTED;
  } else {
    ticl_state_ = TRANSIENT_NOTIFICATION_ERROR;
  }
  EmitStateChange();
}

void ChromeInvalidationClient::WriteState(const std::string& state) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "WriteState";
  invalidation_state_tracker_.Call(
      FROM_HERE, &InvalidationStateTracker::SetInvalidationState, state);
}

void ChromeInvalidationClient::Stop() {
  DCHECK(CalledOnValidThread());
  if (!invalidation_client_.get()) {
    return;
  }

  registration_manager_.reset();
  chrome_system_resources_.Stop();
  invalidation_client_->Stop();

  invalidation_client_.reset();
  listener_ = NULL;

  invalidation_state_tracker_.Reset();
  max_invalidation_versions_.clear();
  ticl_state_ = DEFAULT_NOTIFICATION_ERROR;
  push_client_state_ = DEFAULT_NOTIFICATION_ERROR;
}

NotificationsDisabledReason ChromeInvalidationClient::GetState() const {
  DCHECK(CalledOnValidThread());
  if (ticl_state_ == NOTIFICATION_CREDENTIALS_REJECTED ||
      push_client_state_ == NOTIFICATION_CREDENTIALS_REJECTED) {
    // If either the ticl or the push client rejected our credentials,
    // return NOTIFICATION_CREDENTIALS_REJECTED.
    return NOTIFICATION_CREDENTIALS_REJECTED;
  }
  if (ticl_state_ == NO_NOTIFICATION_ERROR &&
      push_client_state_ == NO_NOTIFICATION_ERROR) {
    // If the ticl is ready and the push client notifications are
    // enabled, return NO_NOTIFICATION_ERROR.
    return NO_NOTIFICATION_ERROR;
  }
  // Otherwise, we have a transient error.
  return TRANSIENT_NOTIFICATION_ERROR;
}

void ChromeInvalidationClient::EmitStateChange() {
  DCHECK(CalledOnValidThread());
  if (GetState() == NO_NOTIFICATION_ERROR) {
    listener_->OnNotificationsEnabled();
  } else {
    listener_->OnNotificationsDisabled(GetState());
  }
}

void ChromeInvalidationClient::OnNotificationsEnabled() {
  DCHECK(CalledOnValidThread());
  push_client_state_ = NO_NOTIFICATION_ERROR;
  EmitStateChange();
}

void ChromeInvalidationClient::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  DCHECK(CalledOnValidThread());
  push_client_state_ = FromNotifierReason(reason);
  EmitStateChange();
}

void ChromeInvalidationClient::OnIncomingNotification(
    const notifier::Notification& notification) {
  DCHECK(CalledOnValidThread());
  // Do nothing, since this is already handled by |invalidation_client_|.
}

}  // namespace csync
