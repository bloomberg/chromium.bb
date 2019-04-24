// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_util.h"

#include <string>

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/user_manager/user_manager.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/chromeos/events/keyboard_layout_util.h"

namespace assistant {

ash::mojom::AssistantAllowedState IsAssistantAllowedForProfile(
    const Profile* profile) {
  if (!chromeos::switches::IsAssistantEnabled())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_FLAG;

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER;

  if (profile->IsOffTheRecord())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_INCOGNITO;

  if (profile->IsLegacySupervised())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER;

  if (profile->IsChild())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_CHILD_USER;

  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE;

  if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION;

  // TODO(wutao): Add a new type DISALLOWED_BY_KIOSK_MODE.
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp() ||
      user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp()) {
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE;
  }

  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA,
                                         ULOC_CANADA_FRENCH,
                                         ULOC_FRANCE,
                                         ULOC_FRENCH,
                                         ULOC_UK,
                                         ULOC_US,
                                         "da",
                                         "nb",
                                         "nl",
                                         "nn",
                                         "no",
                                         "sv"};

  const PrefService* prefs = profile->GetPrefs();
  std::string pref_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  // Also accept runtime locale which maybe an approximation of user's pref
  // locale.
  const std::string kRuntimeLocale = icu::Locale::getDefault().getName();
  if (!pref_locale.empty()) {
    base::ReplaceChars(pref_locale, "-", "_", &pref_locale);
    bool disallowed = !base::ContainsValue(kAllowedLocales, pref_locale) &&
                      !base::ContainsValue(kAllowedLocales, kRuntimeLocale);

    if (disallowed)
      return ash::mojom::AssistantAllowedState::DISALLOWED_BY_LOCALE;
  }

  // Bypass the account type check when using fake gaia login, e.g. in Tast
  // tests, or the account is logged in a device with a physical Assistant key
  // on keyboard.
  if (!chromeos::switches::IsGaiaServicesDisabled() &&
      !ui::DeviceUsesKeyboardLayout2()) {
    // Only enable non-dasher accounts for devices without physical key.
    bool account_supported = false;
    auto* identity_manager =
        IdentityManagerFactory::GetForProfileIfExists(profile);

    if (identity_manager) {
      const std::string hosted_domain =
          identity_manager->GetPrimaryAccountInfo().hosted_domain;
      // |kNoHostedDomainFound| means it's gmail.com accounts.
      if (hosted_domain == kNoHostedDomainFound ||
          hosted_domain == "google.com") {
        account_supported = true;
      }
    }

    if (!account_supported)
      return ash::mojom::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE;
  }

  return ash::mojom::AssistantAllowedState::ALLOWED;
}

}  // namespace assistant
