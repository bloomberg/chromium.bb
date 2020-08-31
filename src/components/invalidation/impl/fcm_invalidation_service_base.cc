// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service_base.h"

#include <memory>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/topic_data.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace invalidation {
namespace {
const char kApplicationName[] = "com.google.chrome.fcm.invalidations";
// Sender ID coming from the Firebase console.
const char kInvalidationGCMSenderId[] = "8181035976";

// Added in M76.
void MigratePrefs(PrefService* prefs, const std::string& sender_id) {
  if (!prefs->HasPrefPath(prefs::kFCMInvalidationClientIDCacheDeprecated)) {
    return;
  }
  DictionaryPrefUpdate update(prefs, prefs::kInvalidationClientIDCache);
  update->SetString(
      sender_id,
      prefs->GetString(prefs::kFCMInvalidationClientIDCacheDeprecated));
  prefs->ClearPref(prefs::kFCMInvalidationClientIDCacheDeprecated);
}

}  // namespace

FCMInvalidationServiceBase::FCMInvalidationServiceBase(
    FCMNetworkHandlerCallback fcm_network_handler_callback,
    PerUserTopicSubscriptionManagerCallback
        per_user_topic_subscription_manager_callback,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const std::string& sender_id)
    : sender_id_(sender_id.empty() ? kInvalidationGCMSenderId : sender_id),
      invalidator_registrar_(pref_service,
                             sender_id_,
                             sender_id_ == kInvalidationGCMSenderId),
      fcm_network_handler_callback_(std::move(fcm_network_handler_callback)),
      per_user_topic_subscription_manager_callback_(
          std::move(per_user_topic_subscription_manager_callback)),
      instance_id_driver_(instance_id_driver),
      pref_service_(pref_service) {}

FCMInvalidationServiceBase::~FCMInvalidationServiceBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_registrar_.UpdateInvalidatorState(
      syncer::INVALIDATOR_SHUTTING_DOWN);

  if (IsStarted())
    StopInvalidator();
}

// static
void FCMInvalidationServiceBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      invalidation::prefs::kFCMInvalidationClientIDCacheDeprecated,
      /*default_value=*/std::string());
  registry->RegisterDictionaryPref(
      invalidation::prefs::kInvalidationClientIDCache);
}

void FCMInvalidationServiceBase::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Registering an invalidation handler";
  invalidator_registrar_.RegisterHandler(handler);
  // Populate the id for newly registered handlers.
  handler->OnInvalidatorClientIdChange(client_id_);
  logger_.OnRegistration(handler->GetOwnerName());
}

bool FCMInvalidationServiceBase::UpdateInterestedTopics(
    syncer::InvalidationHandler* handler,
    const syncer::TopicSet& legacy_topic_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_was_requested_ = true;
  DVLOG(2) << "Subscribing to topics: " << legacy_topic_set.size();
  std::set<TopicData> topic_set;
  for (const auto& topic_name : legacy_topic_set) {
    topic_set.insert(TopicData(topic_name, handler->IsPublicTopic(topic_name)));
  }
  // TODO(crbug.com/1054404): UpdateRegisteredTopics() should be renamed to
  // clarify that it actually updates whether topics need subscription (aka
  // interested).
  if (!invalidator_registrar_.UpdateRegisteredTopics(handler, topic_set)) {
    return false;
  }
  DoUpdateSubscribedTopicsIfNeeded();
  logger_.OnUpdatedTopics(invalidator_registrar_.GetHandlerNameToTopicsMap());
  return true;
}

void FCMInvalidationServiceBase::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Unregistering";
  invalidator_registrar_.UnregisterHandler(handler);
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState FCMInvalidationServiceBase::GetInvalidatorState()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (invalidation_listener_) {
    DVLOG(2) << "GetInvalidatorState returning "
             << invalidator_registrar_.GetInvalidatorState();
    return invalidator_registrar_.GetInvalidatorState();
  }
  DVLOG(2) << "Invalidator currently stopped";
  return syncer::STOPPED;
}

std::string FCMInvalidationServiceBase::GetInvalidatorClientId() const {
  return client_id_;
}

InvalidationLogger* FCMInvalidationServiceBase::GetInvalidationLogger() {
  return &logger_;
}

void FCMInvalidationServiceBase::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> return_callback)
    const {
  return_callback.Run(CollectDebugData());
  invalidator_registrar_.RequestDetailedStatus(return_callback);

  if (IsStarted()) {
    invalidation_listener_->RequestDetailedStatus(return_callback);
  }
}

void FCMInvalidationServiceBase::OnInvalidate(
    const syncer::TopicInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);

  logger_.OnInvalidation(invalidation_map);
}

void FCMInvalidationServiceBase::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  ReportInvalidatorState(state);
  invalidator_registrar_.UpdateInvalidatorState(state);
  logger_.OnStateChange(state);
}

void FCMInvalidationServiceBase::InitForTest(
    std::unique_ptr<syncer::FCMInvalidationListener> invalidation_listener) {
  // Here we perform the equivalent of Init() and StartInvalidator(), but with
  // some minor changes to account for the fact that we're injecting the
  // invalidation_listener.

  // StartInvalidator initializes the invalidation_listener and starts it.
  invalidation_listener_ = std::move(invalidation_listener);
  invalidation_listener_->StartForTest(this);

  PopulateClientID();
  DoUpdateSubscribedTopicsIfNeeded();
}

base::DictionaryValue FCMInvalidationServiceBase::CollectDebugData() const {
  base::DictionaryValue status;

  status.SetString(
      "InvalidationService.IID-requested",
      base::TimeFormatShortDateAndTime(diagnostic_info_.instance_id_requested));
  status.SetString(
      "InvalidationService.IID-received",
      base::TimeFormatShortDateAndTime(diagnostic_info_.instance_id_received));
  status.SetString(
      "InvalidationService.IID-cleared",
      base::TimeFormatShortDateAndTime(diagnostic_info_.instance_id_cleared));
  status.SetString(
      "InvalidationService.Service-stopped",
      base::TimeFormatShortDateAndTime(diagnostic_info_.service_was_stopped));
  status.SetString(
      "InvalidationService.Service-started",
      base::TimeFormatShortDateAndTime(diagnostic_info_.service_was_started));
  return status;
}

void FCMInvalidationServiceBase::ReportInvalidatorState(
    syncer::InvalidatorState state) {
  base::UmaHistogramEnumeration("Invalidations.StatusChanged", state);
}

bool FCMInvalidationServiceBase::IsStarted() const {
  return invalidation_listener_ != nullptr;
}

void FCMInvalidationServiceBase::StartInvalidator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!invalidation_listener_);

  diagnostic_info_.service_was_started = base::Time::Now();
  auto network =
      fcm_network_handler_callback_.Run(sender_id_, GetApplicationName());
  // The order of calls is important. Do not change.
  // We should start listening before requesting the id, because
  // valid id is only generated, once there is an app handler
  // for the app. StartListening registers the app handler.
  // We should create InvalidationListener first, because it registers the
  // handler for the incoming messages, which is crucial on Android, because on
  // the startup cached messages might exists.
  invalidation_listener_ =
      std::make_unique<syncer::FCMInvalidationListener>(std::move(network));
  auto subscription_manager = per_user_topic_subscription_manager_callback_.Run(
      sender_id_, /*migrate_prefs=*/sender_id_ == kInvalidationGCMSenderId);
  invalidation_listener_->Start(this, std::move(subscription_manager));

  PopulateClientID();
  DoUpdateSubscribedTopicsIfNeeded();
}

void FCMInvalidationServiceBase::StopInvalidator() {
  DCHECK(invalidation_listener_);
  diagnostic_info_.service_was_stopped = base::Time::Now();
  invalidation_listener_.reset();
}

void FCMInvalidationServiceBase::StopInvalidatorPermanently() {
  // Reset the client ID (aka InstanceID) *before* stopping, so that
  // FCMInvalidationListener gets notified about the cleared ID (the listener
  // gets destroyed during StopInvalidator()).
  if (!client_id_.empty())
    ResetClientID();

  StopInvalidator();
}

void FCMInvalidationServiceBase::PopulateClientID() {
  diagnostic_info_.instance_id_requested = base::Time::Now();
  if (sender_id_ == kInvalidationGCMSenderId) {
    MigratePrefs(pref_service_, sender_id_);
  }
  const std::string* client_id_pref =
      pref_service_->GetDictionary(prefs::kInvalidationClientIDCache)
          ->FindStringKey(sender_id_);
  client_id_ = client_id_pref ? *client_id_pref : "";
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(GetApplicationName());
  instance_id->GetID(
      base::BindOnce(&FCMInvalidationServiceBase::OnInstanceIDReceived,
                     base::Unretained(this)));
}

void FCMInvalidationServiceBase::ResetClientID() {
  instance_id::InstanceID* instance_id =
      instance_id_driver_->GetInstanceID(GetApplicationName());
  instance_id->DeleteID(
      base::BindOnce(&FCMInvalidationServiceBase::OnDeleteInstanceIDCompleted,
                     base::Unretained(this)));

  // Immediately clear our cached values (before we get confirmation of the
  // deletion), since they shouldn't be used anymore. Lower layers are the
  // source of truth, and are responsible for ensuring that the deletion
  // actually happens.
  client_id_.clear();
  DictionaryPrefUpdate update(pref_service_, prefs::kInvalidationClientIDCache);
  update->RemoveKey(sender_id_);

  // Also let the registrar (and its observers) know that the instance ID is
  // gone.
  invalidator_registrar_.UpdateInvalidatorInstanceId(std::string());

  // This will also delete all Instance ID *tokens*; we need to let the
  // FCMInvalidationListener know.
  if (invalidation_listener_) {
    invalidation_listener_->ClearInstanceIDToken();
  }
}

void FCMInvalidationServiceBase::OnInstanceIDReceived(
    const std::string& instance_id) {
  diagnostic_info_.instance_id_received = base::Time::Now();
  if (client_id_ != instance_id) {
    client_id_ = instance_id;
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kInvalidationClientIDCache);
    update->SetStringKey(sender_id_, instance_id);
    invalidator_registrar_.UpdateInvalidatorInstanceId(instance_id);
  }
}

void FCMInvalidationServiceBase::OnDeleteInstanceIDCompleted(
    instance_id::InstanceID::Result result) {
  // Note: |client_id_| and the pref were already cleared when we initiated the
  // deletion.

  base::UmaHistogramEnumeration("FCMInvalidations.ResetClientIDStatus", result);

  diagnostic_info_.instance_id_cleared = base::Time::Now();
}

void FCMInvalidationServiceBase::DoUpdateSubscribedTopicsIfNeeded() {
  if (!invalidation_listener_ || !update_was_requested_)
    return;
  auto subscribed_topics = invalidator_registrar_.GetAllSubscribedTopics();
  invalidation_listener_->UpdateInterestedTopics(subscribed_topics);
  update_was_requested_ = false;
}

const std::string FCMInvalidationServiceBase::GetApplicationName() {
  // If using the default |sender_id_|, use the bare |kApplicationName|, so the
  // old app name is maintained.
  if (sender_id_ == kInvalidationGCMSenderId) {
    return kApplicationName;
  }
  return base::StrCat({kApplicationName, "-", sender_id_});
}

}  // namespace invalidation
