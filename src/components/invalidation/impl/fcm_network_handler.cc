// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include "base/base64url.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/status.h"
#include "components/invalidation/public/invalidator_state.h"

using instance_id::InstanceID;

namespace syncer {

namespace {

const char kInvalidationsAppId[] = "com.google.chrome.fcm.invalidations";
const char kInvalidationGCMSenderId[] = "8181035976";
const char kPayloadKey[] = "payload";
const char kPublicTopic[] = "external_name";
const char kVersionKey[] = "version";

// OAuth2 Scope passed to getToken to obtain GCM registration tokens.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

// Lower bound time between two token validations when listening.
const int kTokenValidationPeriodMinutesDefault = 60 * 24;

std::string GetValueFromMessage(const gcm::IncomingMessage& message,
                                const std::string& key) {
  std::string value;
  auto it = message.data.find(key);
  if (it != message.data.end())
    value = it->second;
  return value;
}

InvalidationParsingStatus ParseIncommingMessage(
    const gcm::IncomingMessage& message,
    std::string* payload,
    std::string* private_topic,
    std::string* public_topic,
    std::string* version) {
  *payload = GetValueFromMessage(message, kPayloadKey);
  *version = GetValueFromMessage(message, kVersionKey);

  // Version must always be there.
  if (version->empty())
    return InvalidationParsingStatus::kVersionEmpty;

  *public_topic = GetValueFromMessage(message, kPublicTopic);

  // Public topic must always be there.
  if (public_topic->empty())
    return InvalidationParsingStatus::kPublicTopicEmpty;

  *private_topic = message.sender_id;
  if (private_topic->empty())
    return InvalidationParsingStatus::kPrivateTopicEmpty;

  return InvalidationParsingStatus::kSuccess;
}

}  // namespace

FCMNetworkHandler::FCMNetworkHandler(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      token_validation_timer_(std::make_unique<base::OneShotTimer>()),
      weak_ptr_factory_(this) {}

FCMNetworkHandler::~FCMNetworkHandler() {
  StopListening();
}

void FCMNetworkHandler::StartListening() {
  // Adding ourselves as Handler means start listening.
  // Being the listener is pre-requirement for token operations.
  gcm_driver_->AddAppHandler(kInvalidationsAppId, this);

  instance_id_driver_->GetInstanceID(kInvalidationsAppId)
      ->GetToken(kInvalidationGCMSenderId, kGCMScope,
                 /*options=*/std::map<std::string, std::string>(),
                 /*is_lazy=*/true,
                 base::BindRepeating(&FCMNetworkHandler::DidRetrieveToken,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::StopListening() {
  if (IsListening())
    gcm_driver_->RemoveAppHandler(kInvalidationsAppId);
}

bool FCMNetworkHandler::IsListening() const {
  return gcm_driver_->GetAppHandler(kInvalidationsAppId);
}

void FCMNetworkHandler::DidRetrieveToken(const std::string& subscription_token,
                                         InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      // The received token is assumed to be valid, therefore, we reschedule
      // validation.
      DeliverToken(subscription_token);
      token_ = subscription_token;
      break;
    case InstanceID::INVALID_PARAMETER:
    case InstanceID::DISABLED:
    case InstanceID::ASYNC_OPERATION_PENDING:
    case InstanceID::SERVER_ERROR:
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::NETWORK_ERROR:
      DLOG(WARNING) << "Messaging subscription failed; InstanceID::Result = "
                    << result;
      UpdateGcmChannelState(false);
      break;
  }
  ScheduleNextTokenValidation();
}

void FCMNetworkHandler::ScheduleNextTokenValidation() {
  DCHECK(IsListening());

  token_validation_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMinutes(kTokenValidationPeriodMinutesDefault),
      base::BindOnce(&FCMNetworkHandler::StartTokenValidation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::StartTokenValidation() {
  DCHECK(IsListening());

  instance_id_driver_->GetInstanceID(kInvalidationsAppId)
      ->GetToken(kInvalidationGCMSenderId, kGCMScope,
                 std::map<std::string, std::string>(),
                 /*is_lazy=*/true,
                 base::Bind(&FCMNetworkHandler::DidReceiveTokenForValidation,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FCMNetworkHandler::DidReceiveTokenForValidation(
    const std::string& new_token,
    InstanceID::Result result) {
  if (!IsListening()) {
    // After we requested the token, |StopListening| has been called. Thus,
    // ignore the token.
    return;
  }

  if (result == InstanceID::SUCCESS) {
    if (token_ != new_token) {
      token_ = new_token;
      DeliverToken(new_token);
    }
  }

  ScheduleNextTokenValidation();
}

void FCMNetworkHandler::UpdateGcmChannelState(bool online) {
  if (gcm_channel_online_ == online)
    return;
  gcm_channel_online_ = online;
  NotifyChannelStateChange(gcm_channel_online_ ? INVALIDATIONS_ENABLED
                                               : TRANSIENT_INVALIDATION_ERROR);
}

void FCMNetworkHandler::ShutdownHandler() {}

void FCMNetworkHandler::OnStoreReset() {}

void FCMNetworkHandler::OnMessage(const std::string& app_id,
                                  const gcm::IncomingMessage& message) {
  DCHECK_EQ(app_id, kInvalidationsAppId);
  std::string payload;
  std::string private_topic;
  std::string public_topic;
  std::string version;

  InvalidationParsingStatus status = ParseIncommingMessage(
      message, &payload, &private_topic, &public_topic, &version);
  UMA_HISTOGRAM_ENUMERATION("FCMInvalidations.FCMMessageStatus", status);

  UpdateGcmChannelState(true);

  if (status == InvalidationParsingStatus::kSuccess)
    DeliverIncomingMessage(payload, private_topic, public_topic, version);
}

void FCMNetworkHandler::OnMessagesDeleted(const std::string& app_id) {
  // TODO(melandory): consider notifyint the client that messages were
  // deleted. So the client can act on it, e.g. in case of sync request
  // GetUpdates from the server.
}

void FCMNetworkHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::SetTokenValidationTimerForTesting(
    std::unique_ptr<base::OneShotTimer> token_validation_timer) {
  token_validation_timer_ = std::move(token_validation_timer);
}

}  // namespace syncer
