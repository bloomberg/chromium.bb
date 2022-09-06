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
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace {

// Remembers if the main user is managed or not.
// Note: This is a pessimistic default (no policies read - false) and
// once the profile is loaded, the value is set and will never change in
// production. The value changes in tests whenever policy data gets overridden.
bool g_is_main_user_managed_ = false;

enterprise_management::PolicyData* MainUserPolicyDataStorage() {
  static enterprise_management::PolicyData policy_data;
  return &policy_data;
}

bool IsManaged(const enterprise_management::PolicyData& policy_data) {
  return policy_data.state() == enterprise_management::PolicyData::ACTIVE;
}

// Returns whether a primary device account for this session is child.
bool IsChildSession() {
  const crosapi::mojom::BrowserInitParams* init_params =
      chromeos::BrowserInitParams::Get();
  if (!init_params) {
    return false;
  }
  return init_params->session_type ==
         crosapi::mojom::SessionType::kChildSession;
}

}  // namespace

namespace policy {

PolicyLoaderLacros::PolicyLoaderLacros(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PolicyPerProfileFilter per_profile)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/false),
      per_profile_(per_profile) {
  const crosapi::mojom::BrowserInitParams* init_params =
      chromeos::BrowserInitParams::Get();
  if (!init_params) {
    LOG(ERROR) << "No init params";
    return;
  }
  if (per_profile_ == PolicyPerProfileFilter::kTrue &&
      init_params->device_account_component_policy) {
    SetComponentPolicy(init_params->device_account_component_policy.value());
  }
  if (!init_params->device_account_policy) {
    LOG(ERROR) << "No policy data";
    return;
  }
  policy_fetch_response_ = init_params->device_account_policy.value();
  last_fetch_timestamp_ =
      base::Time::FromTimeT(init_params->last_policy_fetch_attempt_timestamp);
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

  // If per_profile loader is used, apply policy for extensions.
  if (per_profile_ == PolicyPerProfileFilter::kTrue && component_policy_)
    bundle->MergeFrom(*component_policy_);

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

  // We do not set enterprise defaults for child accounts, because they are
  // consumer users. The same rule is applied to policy in Ash. See
  // UserCloudPolicyManagerAsh.
  if (!IsChildSession()) {
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
  }
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .MergeFrom(policy_map);

  // Remember if the policy is managed or not.
  g_is_main_user_managed_ = IsManaged(*validator.policy_data());
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

void PolicyLoaderLacros::OnPolicyFetchAttempt() {
  last_fetch_timestamp_ = base::Time::Now();
}

void PolicyLoaderLacros::OnComponentPolicyUpdated(
    const policy::ComponentPolicyMap& component_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The component policy is per_profile=true policy. If Lacros is using
  // secondary profile, that policy is loaded directly from DMServer. In case
  // it is using the device account, there are two PolicyLoaderLacros objects
  // present, and we need to store it only in the object with per_profile:True.
  if (per_profile_ == PolicyPerProfileFilter::kFalse) {
    return;
  }

  SetComponentPolicy(component_policy);
  Reload(true);
}

void PolicyLoaderLacros::SetComponentPolicy(
    const policy::ComponentPolicyMap& component_policy) {
  if (component_policy_) {
    component_policy_->Clear();
  } else {
    component_policy_ = std::make_unique<PolicyBundle>();
  }
  for (auto& policy_pair : component_policy) {
    PolicyMap component_policy_map;
    std::string error;
    // The component policy received from Ash is the JSON data corresponding to
    // the policy for the namespace.
    ParseComponentPolicy(policy_pair.second.Clone(), POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD_FROM_ASH, &component_policy_map,
                         &error);
    DCHECK(error.empty());

    // The data is also good; expose the policies.
    component_policy_->Get(policy_pair.first).Swap(&component_policy_map);
  }
}

enterprise_management::PolicyData* PolicyLoaderLacros::GetPolicyData() {
  if (!policy_fetch_response_ || !policy_data_)
    return nullptr;

  return policy_data_.get();
}

// static
bool PolicyLoaderLacros::IsDeviceLocalAccountUser() {
  const crosapi::mojom::BrowserInitParams* init_params =
      chromeos::BrowserInitParams::Get();
  if (!init_params) {
    return false;
  }
  crosapi::mojom::SessionType session_type = init_params->session_type;
  return session_type == crosapi::mojom::SessionType::kPublicSession ||
         session_type == crosapi::mojom::SessionType::kWebKioskSession ||
         session_type == crosapi::mojom::SessionType::kAppKioskSession;
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
      chromeos::BrowserInitParams::Get();

  // To align with `DeviceLocalAccountUserBase::IsAffiliated()`, a device local
  // account user is always treated as affiliated.
  if (IsDeviceLocalAccountUser()) {
    return true;
  }

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
  g_is_main_user_managed_ = IsManaged(policy_data);
}

}  // namespace policy
