// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace {

// Remembers if the main user is managed or not.
// Note: This is a pessimistic default (no policies read - false) and
// once the profile is loaded, the value is set and will never change.
bool g_is_main_user_managed_ = false;

enterprise_management::PolicyData* MainUserPolicyDataStorage() {
  static enterprise_management::PolicyData policy_data;
  return &policy_data;
}

}  // namespace

namespace policy {

PolicyLoaderLacros::PolicyLoaderLacros(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PolicyPerProfileFilter per_profile)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/false),
      per_profile_(per_profile) {
  auto* lacros_service = chromeos::LacrosService::Get();
  const crosapi::mojom::BrowserInitParams* init_params =
      lacros_service->init_params();
  if (!init_params) {
    LOG(ERROR) << "No init params";
    return;
  }
  if (!init_params->device_account_policy) {
    LOG(ERROR) << "No policy data";
    return;
  }
  policy_fetch_response_ = init_params->device_account_policy.value();
}

PolicyLoaderLacros::~PolicyLoaderLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service) {
    lacros_service->RemoveObserver(this);
  }
}

void PolicyLoaderLacros::InitOnBackgroundThread() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // We add this as observer on background thread to avoid a situation when
  // notification comes after the object is destroyed, but not removed from the
  // list yet.
  // TODO(crbug.com/1114069): Set up LacrosService in tests.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service) {
    lacros_service->AddObserver(this);
  }
}

std::unique_ptr<PolicyBundle> PolicyLoaderLacros::Load() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();

  if (!policy_fetch_response_ || policy_fetch_response_->empty()) {
    return bundle;
  }

  auto policy = std::make_unique<enterprise_management::PolicyFetchResponse>();
  if (!policy->ParseFromArray(policy_fetch_response_.value().data(),
                              policy_fetch_response_->size())) {
    LOG(ERROR) << "Failed to parse policy data";
    return bundle;
  }
  UserCloudPolicyValidator validator(std::move(policy),
                                     /*background_task_runner=*/nullptr);
  validator.ValidatePayload();
  validator.RunValidation();

  PolicyMap policy_map;
  base::WeakPtr<CloudExternalDataManager> external_data_manager;
  DecodeProtoFields(*(validator.payload()), external_data_manager,
                    PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH,
                    PolicyScope::POLICY_SCOPE_USER, &policy_map, per_profile_);
  switch (per_profile_) {
    case PolicyPerProfileFilter::kTrue:
      SetEnterpriseUsersProfileDefaults(&policy_map);
      break;
    case PolicyPerProfileFilter::kFalse:
      SetEnterpriseUsersSystemWideDefaults(&policy_map);
      break;
    case PolicyPerProfileFilter::kAny:
      NOTREACHED();
  }
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .MergeFrom(policy_map);

  // Remember if the policy is managed or not.
  g_is_main_user_managed_ = validator.policy_data()->state() ==
                            enterprise_management::PolicyData::ACTIVE;
  if (g_is_main_user_managed_ &&
      per_profile_ == PolicyPerProfileFilter::kFalse) {
    *MainUserPolicyDataStorage() = *validator.policy_data();
  }
  policy_data_ = std::move(validator.policy_data());

  return bundle;
}

void PolicyLoaderLacros::OnPolicyUpdated(
    const std::vector<uint8_t>& policy_fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy_fetch_response_ = policy_fetch_response;
  Reload(true);
}

enterprise_management::PolicyData* PolicyLoaderLacros::GetPolicyData() {
  if (!policy_fetch_response_ || !policy_data_)
    return nullptr;

  return policy_data_.get();
}

// static
bool PolicyLoaderLacros::IsMainUserManaged() {
  return g_is_main_user_managed_;
}

// static
bool PolicyLoaderLacros::IsMainUserAffiliated() {
  const enterprise_management::PolicyData* policy =
      policy::PolicyLoaderLacros::main_user_policy_data();
  const crosapi::mojom::BrowserInitParams* init_params =
      chromeos::LacrosService::Get()->init_params();
  if (policy && !policy->user_affiliation_ids().empty() && init_params &&
      init_params->device_properties &&
      init_params->device_properties->device_affiliation_ids.has_value()) {
    const auto& user_ids = policy->user_affiliation_ids();
    const auto& device_ids =
        init_params->device_properties->device_affiliation_ids.value();
    return policy::IsAffiliated({user_ids.begin(), user_ids.end()},
                                {device_ids.begin(), device_ids.end()});
  }
  return false;
}

// static
const enterprise_management::PolicyData*
PolicyLoaderLacros::main_user_policy_data() {
  return MainUserPolicyDataStorage();
}

// static
void PolicyLoaderLacros::set_main_user_policy_data_for_testing(
    const enterprise_management::PolicyData& policy_data) {
  *MainUserPolicyDataStorage() = policy_data;
}

}  // namespace policy
