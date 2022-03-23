// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace {

bool PrivacySandboxRestrictedByAcccountCapability(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager)
    return false;

  const auto core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  auto capability =
      account_info.capabilities.can_run_chrome_privacy_sandbox_trials();

  // The Privacy Sandbox is not considered restricted unless the capability
  // has a definitive false signal.
  return capability == signin::Tribool::kFalse;
}

}  // namespace

PrivacySandboxSettingsDelegate::PrivacySandboxSettingsDelegate(Profile* profile)
    : profile_(profile) {}

PrivacySandboxSettingsDelegate::~PrivacySandboxSettingsDelegate() = default;

bool PrivacySandboxSettingsDelegate::IsPrivacySandboxRestricted() {
  // When the Privacy Sandbox 3 feature is enabled, the Sandbox is restricted
  // for Child users.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    return profile_->IsChild() ||
           PrivacySandboxRestrictedByAcccountCapability(profile_);
  }
  // No restrictions apply otherwise.
  return false;
}
