// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {
struct DevicePolicyToUserPolicyMapEntry {
  const char* const device_policy_name;
  const char* const user_policy_name;
};

const char kLidCloseAction[] = "LidCloseAction";
const char kUserActivityScreenDimDelayScale[] =
    "UserActivityScreenDimDelayScale";

const char kActionSuspend[] = "Suspend";
const char kActionLogout[] = "Logout";
const char kActionShutdown[]  = "Shutdown";
const char kActionDoNothing[] = "DoNothing";

const DevicePolicyToUserPolicyMapEntry kDevicePoliciesWithPolicyOptionsMap[] = {
    {key::kDeviceLoginScreenAutoSelectCertificateForUrls,
     key::kAutoSelectCertificateForUrls},
    {key::kDeviceLoginScreenLargeCursorEnabled, key::kLargeCursorEnabled},
    {key::kDeviceLoginScreenSpokenFeedbackEnabled, key::kSpokenFeedbackEnabled},
    {key::kDeviceLoginScreenHighContrastEnabled, key::kHighContrastEnabled},
    {key::kDeviceLoginScreenVirtualKeyboardEnabled,
     key::kVirtualKeyboardEnabled},
    {key::kDeviceLoginScreenDictationEnabled, key::kDictationEnabled},
    {key::kDeviceLoginScreenSelectToSpeakEnabled, key::kSelectToSpeakEnabled},
    {key::kDeviceLoginScreenCursorHighlightEnabled,
     key::kCursorHighlightEnabled},
    {key::kDeviceLoginScreenCaretHighlightEnabled, key::kCaretHighlightEnabled},
    {key::kDeviceLoginScreenMonoAudioEnabled, key::kMonoAudioEnabled},
    {key::kDeviceLoginScreenAutoclickEnabled, key::kAutoclickEnabled},
    {key::kDeviceLoginScreenStickyKeysEnabled, key::kStickyKeysEnabled},
    {key::kDeviceLoginScreenKeyboardFocusHighlightEnabled,
     key::kKeyboardFocusHighlightEnabled},
    {key::kDeviceLoginScreenScreenMagnifierType, key::kScreenMagnifierType},
    {key::kDeviceLoginScreenShowOptionsInSystemTrayMenu,
     key::kShowAccessibilityOptionsInSystemTrayMenu},
    {key::kDeviceLoginScreenPrimaryMouseButtonSwitch,
     key::kPrimaryMouseButtonSwitch},
    {key::kDeviceLoginScreenAccessibilityShortcutsEnabled,
     key::kAccessibilityShortcutsEnabled},
    {key::kDeviceLoginScreenPrivacyScreenEnabled, key::kPrivacyScreenEnabled},
};

const DevicePolicyToUserPolicyMapEntry kRecommendedDevicePoliciesMap[] = {
    {key::kDeviceLoginScreenDefaultLargeCursorEnabled,
     key::kLargeCursorEnabled},
    {key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
     key::kSpokenFeedbackEnabled},
    {key::kDeviceLoginScreenDefaultHighContrastEnabled,
     key::kHighContrastEnabled},
    {key::kDeviceLoginScreenDefaultScreenMagnifierType,
     key::kScreenMagnifierType},
    {key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
     key::kVirtualKeyboardEnabled},
};

std::unique_ptr<base::Value> GetAction(const std::string& action) {
  if (action == kActionSuspend) {
    return std::unique_ptr<base::Value>(
        new base::Value(chromeos::PowerPolicyController::ACTION_SUSPEND));
  }
  if (action == kActionLogout) {
    return std::unique_ptr<base::Value>(
        new base::Value(chromeos::PowerPolicyController::ACTION_STOP_SESSION));
  }
  if (action == kActionShutdown) {
    return std::unique_ptr<base::Value>(
        new base::Value(chromeos::PowerPolicyController::ACTION_SHUT_DOWN));
  }
  if (action == kActionDoNothing) {
    return std::unique_ptr<base::Value>(
        new base::Value(chromeos::PowerPolicyController::ACTION_DO_NOTHING));
  }
  return std::unique_ptr<base::Value>();
}

// Applies |value| as the recommended value of |user_policy| in
// |user_policy_map|. If |value| is nullptr, does nothing.
void ApplyValueAsRecommendedPolicy(const base::Value* value,
                                   const std::string& user_policy,
                                   PolicyMap* user_policy_map) {
  if (value) {
    user_policy_map->Set(user_policy, POLICY_LEVEL_RECOMMENDED,
                         POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                         value->CreateDeepCopy(), nullptr);
  }
}

// Applies the value of |device_policy| in |device_policy_map| as the
// recommended value of |user_policy| in |user_policy_map|. If the value of
// |device_policy| is unset, does nothing.
void ApplyDevicePolicyAsRecommendedPolicy(const std::string& device_policy,
                                          const std::string& user_policy,
                                          const PolicyMap& device_policy_map,
                                          PolicyMap* user_policy_map) {
  const base::Value* value = device_policy_map.GetValue(device_policy);
  ApplyValueAsRecommendedPolicy(value, user_policy, user_policy_map);
}

// Applies |value| as the mandatory value of |user_policy| in |user_policy_map|.
// If |value| is NULL, does nothing.
void ApplyValueAsMandatoryPolicy(const base::Value* value,
                                 const std::string& user_policy,
                                 PolicyMap* user_policy_map) {
  if (value) {
    user_policy_map->Set(user_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, value->CreateDeepCopy(), nullptr);
  }
}

void ApplyDevicePolicyWithPolicyOptions(const std::string& device_policy,
                                        const std::string& user_policy,
                                        const PolicyMap& device_policy_map,
                                        PolicyMap* user_policy_map) {
  const PolicyMap::Entry* entry = device_policy_map.Get(device_policy);
  if (entry) {
    user_policy_map->Set(user_policy, entry->level, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, entry->value->CreateDeepCopy(),
                         nullptr);
  }
}
}  // namespace

LoginProfilePolicyProvider::LoginProfilePolicyProvider(
    PolicyService* device_policy_service)
    : device_policy_service_(device_policy_service),
      waiting_for_device_policy_refresh_(false) {}

LoginProfilePolicyProvider::~LoginProfilePolicyProvider() {
}

void LoginProfilePolicyProvider::Init(SchemaRegistry* registry) {
  ConfigurationPolicyProvider::Init(registry);
  device_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME))
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::Shutdown() {
  device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  weak_factory_.InvalidateWeakPtrs();
  ConfigurationPolicyProvider::Shutdown();
}

void LoginProfilePolicyProvider::RefreshPolicies() {
  waiting_for_device_policy_refresh_ = true;
  weak_factory_.InvalidateWeakPtrs();
  device_policy_service_->RefreshPolicies(
      base::BindOnce(&LoginProfilePolicyProvider::OnDevicePolicyRefreshDone,
                     weak_factory_.GetWeakPtr()));
}

void LoginProfilePolicyProvider::OnPolicyUpdated(const PolicyNamespace& ns,
                                                 const PolicyMap& previous,
                                                 const PolicyMap& current) {
  if (ns == PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain == POLICY_DOMAIN_CHROME)
    UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::OnDevicePolicyRefreshDone() {
  waiting_for_device_policy_refresh_ = false;
  UpdateFromDevicePolicy();
}

void LoginProfilePolicyProvider::UpdateFromDevicePolicy() {
  // If a policy refresh is in progress, wait for it to finish.
  if (waiting_for_device_policy_refresh_)
    return;

  const PolicyNamespace chrome_namespaces(POLICY_DOMAIN_CHROME, std::string());
  const PolicyMap& device_policy_map =
      device_policy_service_->GetPolicies(chrome_namespaces);
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle);
  PolicyMap& user_policy_map = bundle->Get(chrome_namespaces);

  // The device policies which includes the policy options
  // |kDevicePoliciesWithPolicyOptionsMap| should be applied after
  // |kRecommendedDevicePoliciesMap|, because its overrides some deprecated ones
  // there.
  for (const auto& entry : kRecommendedDevicePoliciesMap) {
    ApplyDevicePolicyAsRecommendedPolicy(entry.device_policy_name,
                                         entry.user_policy_name,
                                         device_policy_map, &user_policy_map);
  }

  for (const auto& entry : kDevicePoliciesWithPolicyOptionsMap) {
    ApplyDevicePolicyWithPolicyOptions(entry.device_policy_name,
                                       entry.user_policy_name,
                                       device_policy_map, &user_policy_map);
  }

  const base::Value* value =
      device_policy_map.GetValue(key::kDeviceLoginScreenPowerManagement);
  const base::DictionaryValue* dict = NULL;
  if (value && value->GetAsDictionary(&dict)) {
    std::unique_ptr<base::DictionaryValue> policy_value(dict->DeepCopy());
    std::string lid_close_action;
    base::Value* screen_dim_delay_scale = NULL;

    if (policy_value->GetString(kLidCloseAction, &lid_close_action)) {
      std::unique_ptr<base::Value> action = GetAction(lid_close_action);
      if (action) {
        ApplyValueAsMandatoryPolicy(
            action.get(), key::kLidCloseAction, &user_policy_map);
      }
      policy_value->Remove(kLidCloseAction, NULL);
    }

    if (policy_value->Get(kUserActivityScreenDimDelayScale,
                          &screen_dim_delay_scale)) {
      ApplyValueAsMandatoryPolicy(screen_dim_delay_scale,
                                  key::kUserActivityScreenDimDelayScale,
                                  &user_policy_map);
      policy_value->Remove(kUserActivityScreenDimDelayScale, NULL);
    }

    // |policy_value| is expected to be a valid value for the
    // PowerManagementIdleSettings policy now.
    if (!policy_value->empty()) {
      ApplyValueAsMandatoryPolicy(policy_value.get(),
                                  key::kPowerManagementIdleSettings,
                                  &user_policy_map);
    }
  }

  UpdatePolicy(std::move(bundle));
}

}  // namespace policy
