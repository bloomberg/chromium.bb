// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_invalidator.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace chromeos {
namespace cert_provisioning {

namespace {

// Shall be expanded to cert.[scope].[topic]
const char* kOwnerNameFormat = "cert.%s.%s";

const char* CertScopeToString(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return "user";
    case CertScope::kDevice:
      return "device";
  }

  NOTREACHED() << "Unknown cert scope: " << static_cast<int>(scope);
  return "";
}

}  // namespace

//=============== CertProvisioningInvalidationHandler ==========================

namespace internal {

// static
std::unique_ptr<CertProvisioningInvalidationHandler>
CertProvisioningInvalidationHandler::BuildAndRegister(
    CertScope scope,
    invalidation::InvalidationService* invalidation_service,
    const syncer::Topic& topic,
    OnInvalidationCallback on_invalidation_callback) {
  auto invalidator = std::make_unique<CertProvisioningInvalidationHandler>(
      scope, invalidation_service, topic, std::move(on_invalidation_callback));

  if (!invalidator->Register()) {
    return nullptr;
  }

  return invalidator;
}

CertProvisioningInvalidationHandler::CertProvisioningInvalidationHandler(
    CertScope scope,
    invalidation::InvalidationService* invalidation_service,
    const syncer::Topic& topic,
    OnInvalidationCallback on_invalidation_callback)
    : scope_(scope),
      invalidation_service_(invalidation_service),
      topic_(topic),
      on_invalidation_callback_(std::move(on_invalidation_callback)) {
  DCHECK(invalidation_service_);
  DCHECK(!on_invalidation_callback_.is_null());
}

CertProvisioningInvalidationHandler::~CertProvisioningInvalidationHandler() {
  // Unregister is not called here so that subscription can be preserved if
  // browser restarts. If subscription is not needed a user must call Unregister
  // explicitly.
}

bool CertProvisioningInvalidationHandler::Register() {
  if (state_.is_registered) {
    return true;
  }

  OnInvalidatorStateChange(invalidation_service_->GetInvalidatorState());

  invalidation_service_observer_.Add(invalidation_service_);

  if (!invalidation_service_->UpdateInterestedTopics(this,
                                                     /*topics=*/{topic_})) {
    LOG(WARNING) << "Failed to register with topic " << topic_;
    return false;
  }

  state_.is_registered = true;
  return true;
}

void CertProvisioningInvalidationHandler::Unregister() {
  if (!state_.is_registered) {
    return;
  }

  // Assuming that updating invalidator's topics with empty set can never fail
  // since failure is only possible non-empty set with topic associated with
  // some other invalidator.
  const bool topics_reset =
      invalidation_service_->UpdateInterestedTopics(this, syncer::TopicSet());
  DCHECK(topics_reset);
  invalidation_service_observer_.Remove(invalidation_service_);

  state_.is_registered = false;
}

void CertProvisioningInvalidationHandler::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_.is_invalidation_service_enabled =
      state == syncer::INVALIDATIONS_ENABLED;
}

void CertProvisioningInvalidationHandler::OnIncomingInvalidation(
    const syncer::TopicInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!state_.is_invalidation_service_enabled) {
    LOG(WARNING) << "Unexpected invalidation received.";
  }

  const syncer::SingleObjectInvalidationSet& list =
      invalidation_map.ForTopic(topic_);
  if (list.IsEmpty()) {
    NOTREACHED() << "Incoming invlaidation does not contain invalidation"
                    " for certificate topic";
    return;
  }

  for (const auto& it : list) {
    it.Acknowledge();
  }

  on_invalidation_callback_.Run();
}

std::string CertProvisioningInvalidationHandler::GetOwnerName() const {
  return base::StringPrintf(kOwnerNameFormat, CertScopeToString(scope_),
                            topic_.c_str());
}

bool CertProvisioningInvalidationHandler::IsPublicTopic(
    const syncer::Topic& /*topic*/) const {
  return false;
}

}  // namespace internal

//=============== CertProvisioningUserInvalidator ==============================

CertProvisioningInvalidator::CertProvisioningInvalidator() = default;
CertProvisioningInvalidator::~CertProvisioningInvalidator() = default;

void CertProvisioningInvalidator::Unregister() {
  if (invalidation_handler_) {
    invalidation_handler_->Unregister();
    invalidation_handler_.reset();
  }
}

//=============== CertProvisioningUserInvalidatorFactory =======================

CertProvisioningUserInvalidatorFactory::CertProvisioningUserInvalidatorFactory(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

std::unique_ptr<CertProvisioningInvalidator>
CertProvisioningUserInvalidatorFactory::Create() {
  return std::make_unique<CertProvisioningUserInvalidator>(profile_);
}

//=============== CertProvisioningUserInvalidator ==============================

CertProvisioningUserInvalidator::CertProvisioningUserInvalidator(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

void CertProvisioningUserInvalidator::Register(
    const syncer::Topic& topic,
    OnInvalidationCallback on_invalidation_callback) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);
  DCHECK(invalidation_provider);
  invalidation::InvalidationService* invalidation_service =
      invalidation_provider->GetInvalidationServiceForCustomSender(
          policy::kPolicyFCMInvalidationSenderID);
  DCHECK(invalidation_service);

  invalidation_handler_ =
      internal::CertProvisioningInvalidationHandler::BuildAndRegister(
          CertScope::kUser, invalidation_service, topic,
          std::move(on_invalidation_callback));
  if (!invalidation_handler_) {
    LOG(ERROR) << "Failed to register for invalidation topic";
  }
}

//=============== CertProvisioningDeviceInvalidatorFactory =====================

CertProvisioningDeviceInvalidatorFactory::
    CertProvisioningDeviceInvalidatorFactory(
        policy::AffiliatedInvalidationServiceProvider* service_provider)
    : service_provider_(service_provider) {
  DCHECK(service_provider_);
}

std::unique_ptr<CertProvisioningInvalidator>
CertProvisioningDeviceInvalidatorFactory::Create() {
  return std::make_unique<CertProvisioningDeviceInvalidator>(service_provider_);
}

//=============== CertProvisioningDeviceInvalidator ============================

CertProvisioningDeviceInvalidator::CertProvisioningDeviceInvalidator(
    policy::AffiliatedInvalidationServiceProvider* service_provider)
    : service_provider_(service_provider) {
  DCHECK(service_provider_);
}

CertProvisioningDeviceInvalidator::~CertProvisioningDeviceInvalidator() {
  service_provider_->UnregisterConsumer(this);
}

void CertProvisioningDeviceInvalidator::Register(
    const syncer::Topic& topic,
    OnInvalidationCallback on_invalidation_callback) {
  topic_ = topic;
  on_invalidation_callback_ = std::move(on_invalidation_callback);
  service_provider_->RegisterConsumer(this);
}

void CertProvisioningDeviceInvalidator::Unregister() {
  service_provider_->UnregisterConsumer(this);
  CertProvisioningInvalidator::Unregister();
}

void CertProvisioningDeviceInvalidator::OnInvalidationServiceSet(
    invalidation::InvalidationService* invalidation_service) {
  if (!invalidation_service) {
    invalidation_handler_.reset();
    return;
  }

  invalidation_handler_ =
      internal::CertProvisioningInvalidationHandler::BuildAndRegister(
          CertScope::kDevice, invalidation_service, topic_,
          on_invalidation_callback_);
  if (!invalidation_handler_) {
    LOG(ERROR) << "Failed to register for invalidation topic";
  }
}

}  // namespace cert_provisioning
}  // namespace chromeos
