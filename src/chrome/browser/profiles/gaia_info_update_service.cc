// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

GAIAInfoUpdateService::GAIAInfoUpdateService(
    signin::IdentityManager* identity_manager,
    ProfileAttributesStorage* profile_attributes_storage,
    const base::FilePath& profile_path,
    PrefService* profile_prefs)
    : identity_manager_(identity_manager),
      profile_attributes_storage_(profile_attributes_storage),
      profile_path_(profile_path),
      profile_prefs_(profile_prefs) {
  identity_manager_->AddObserver(this);

  if (!ShouldUpdatePrimaryAccount()) {
    ClearProfileEntry();
    return;
  }
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }

  gaia_id_of_profile_attribute_entry_ = entry->GetGAIAId();
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() = default;

void GAIAInfoUpdateService::UpdatePrimaryAccount() {
  if (!ShouldUpdatePrimaryAccount())
    return;

  auto unconsented_primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kNotRequired);

  if (!gaia_id_of_profile_attribute_entry_.empty() &&
      unconsented_primary_account_info.gaia !=
          gaia_id_of_profile_attribute_entry_) {
    ClearProfileEntry();
  }

  auto maybe_account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              unconsented_primary_account_info.account_id);
  if (maybe_account_info.has_value())
    UpdatePrimaryAccount(maybe_account_info.value());
}

void GAIAInfoUpdateService::UpdatePrimaryAccount(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }
  gaia_id_of_profile_attribute_entry_ = info.gaia;
  entry->SetGAIAGivenName(base::UTF8ToUTF16(info.given_name));
  entry->SetGAIAName(base::UTF8ToUTF16(info.full_name));

  entry->SetHostedDomain(info.hosted_domain);
  const base::string16 hosted_domain = base::UTF8ToUTF16(info.hosted_domain);
  profile_prefs_->SetString(prefs::kGoogleServicesHostedDomain,
                            base::UTF16ToUTF8(hosted_domain));

  if (info.picture_url == kNoPictureURLFound) {
    entry->SetGAIAPicture(std::string(), gfx::Image());
  } else if (!info.account_image.IsEmpty()) {
    // Only set the image if it is not empty, to avoid clearing the image if we
    // fail to download it on one of the 24 hours interval to refresh the data.
    entry->SetGAIAPicture(info.last_downloaded_image_url_with_size,
                          info.account_image);
  }
}

// static
bool GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(Profile* profile) {
#if defined(OS_CHROMEOS)
  return base::FeatureList::IsEnabled(chromeos::features::kAvatarToolbarButton);
#endif
  return true;
}

void GAIAInfoUpdateService::UpdateAnyAccount(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }

  // These are idempotent, i.e. the second and any further call for the same
  // account info has no further impact.
  entry->AddAccountName(info.full_name);
  entry->AddAccountCategory(info.hosted_domain == kNoHostedDomainFound
                                ? AccountCategory::kConsumer
                                : AccountCategory::kEnterprise);
}

void GAIAInfoUpdateService::ClearProfileEntry() {
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }
  gaia_id_of_profile_attribute_entry_ = "";
  entry->SetGAIAName(base::string16());
  entry->SetGAIAGivenName(base::string16());
  entry->SetGAIAPicture(std::string(), gfx::Image());
  entry->SetHostedDomain(std::string());
  // Unset the cached URL.
  profile_prefs_->ClearPref(prefs::kGoogleServicesHostedDomain);
}

void GAIAInfoUpdateService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void GAIAInfoUpdateService::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  if (unconsented_primary_account_info.gaia.empty()) {
    ClearProfileEntry();
  } else {
    UpdatePrimaryAccount();
  }
}

void GAIAInfoUpdateService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateAnyAccount(info);

  if (!ShouldUpdatePrimaryAccount())
    return;

  CoreAccountInfo account_info = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);

  if (info.account_id != account_info.account_id)
    return;

  UpdatePrimaryAccount(info);
}

void GAIAInfoUpdateService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }

  // We can fully regenerate the info about all accounts only when there are no
  // signed-out accounts. This means that for instance clearing cookies will
  // reset the info.
  if (accounts_in_cookie_jar_info.signed_out_accounts.empty()) {
    entry->ClearAccountNames();
    entry->ClearAccountCategories();

    // Regenerate based on the info from signed-in accounts (if not available
    // now, it will be regenerated soon via OnExtendedAccountInfoUpdated() once
    // downloaded).
    for (gaia::ListedAccount account :
         accounts_in_cookie_jar_info.signed_in_accounts) {
      auto maybe_account_info =
          identity_manager_
              ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
                  account.id);
      if (maybe_account_info.has_value())
        UpdateAnyAccount(*maybe_account_info);
    }
  }
}

bool GAIAInfoUpdateService::ShouldUpdatePrimaryAccount() {
  return identity_manager_->HasPrimaryAccount(
      signin::ConsentLevel::kNotRequired);
}
