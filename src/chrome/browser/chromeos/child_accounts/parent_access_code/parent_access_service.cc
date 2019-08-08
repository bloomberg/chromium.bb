// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace parent_access {

ParentAccessService::Delegate::Delegate() = default;
ParentAccessService::Delegate::~Delegate() = default;

// static
void ParentAccessService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kParentAccessCodeConfig);
}

ParentAccessService::ParentAccessService(
    std::unique_ptr<ConfigSource> config_source)
    : config_source_(std::move(config_source)),
      clock_(base::DefaultClock::GetInstance()) {
  CreateValidators(config_source_->GetConfigSet());
  DCHECK(LoginScreenClient::Get());
  LoginScreenClient::Get()->SetParentAccessDelegate(this);
}

ParentAccessService::~ParentAccessService() {
  if (LoginScreenClient::HasInstance())
    LoginScreenClient::Get()->SetParentAccessDelegate(nullptr);
}

void ParentAccessService::SetDelegate(Delegate* delegate) {
  DCHECK(!(delegate_ && delegate));
  delegate_ = delegate;
}

void ParentAccessService::ValidateParentAccessCode(
    const std::string& access_code,
    ValidateParentAccessCodeCallback callback) {
  bool validation_result = false;
  for (auto& validator : access_code_validators_) {
    if (validator->Validate(access_code, clock_->Now())) {
      validation_result = true;
      break;
    }
  }

  if (delegate_)
    delegate_->OnAccessCodeValidation(validation_result);

  std::move(callback).Run(validation_result);
}

void ParentAccessService::OnConfigChanged(
    const ConfigSource::ConfigSet& configs) {
  CreateValidators(configs);
}

void ParentAccessService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void ParentAccessService::CreateValidators(
    const ConfigSource::ConfigSet& configs) {
  access_code_validators_.clear();

  // Validators are added to |access_code_validators_| in the order that
  // optimizes validation process. Parent access codes will be validated
  // starting from the front of the vector. The correct validation order:
  // * future configuration
  // * current configuration
  // * old configurations
  if (configs.future_config.has_value()) {
    access_code_validators_.push_back(
        std::make_unique<Authenticator>(configs.future_config.value()));
  } else {
    LOG(WARNING) << "No future config for parent access code in the policy";
  }

  if (configs.current_config.has_value()) {
    access_code_validators_.push_back(
        std::make_unique<Authenticator>(configs.current_config.value()));
  } else {
    LOG(WARNING) << "No current config for parent access code in the policy";
  }

  for (const auto& config : configs.old_configs) {
    access_code_validators_.push_back(std::make_unique<Authenticator>(config));
  }

  if (access_code_validators_.empty())
    LOG(ERROR) << "No config to validate parent access available";
}

}  // namespace parent_access
}  // namespace chromeos
