// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace parent_access {

// static
void ParentAccessService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kParentAccessCodeConfig);
}

// static
ParentAccessService& ParentAccessService::Get() {
  static base::NoDestructor<ParentAccessService> instance;
  return *instance;
}

ParentAccessService::ParentAccessService()
    : clock_(base::DefaultClock::GetInstance()) {}

ParentAccessService::~ParentAccessService() = default;

bool ParentAccessService::ValidateParentAccessCode(
    base::Optional<AccountId> account_id,
    const std::string& access_code) {
  bool validation_result = false;
  for (const auto& map_entry : config_source_.config_map()) {
    if (!account_id || account_id == map_entry.first) {
      for (const auto& validator : map_entry.second) {
        if (validator->Validate(access_code, clock_->Now())) {
          validation_result = true;
          break;
        }
      }
    }
    if (validation_result)
      break;
  }

  for (auto& observer : observers_)
    observer.OnAccessCodeValidation(validation_result, account_id);

  return validation_result;
}

void ParentAccessService::LoadConfigForUser(const user_manager::User* user) {
  config_source_.LoadConfigForUser(user);
}

void ParentAccessService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ParentAccessService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ParentAccessService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace parent_access
}  // namespace chromeos
