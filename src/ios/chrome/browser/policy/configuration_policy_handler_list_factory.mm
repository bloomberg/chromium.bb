// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include "base/bind.h"
#include "base/check.h"
#include "components/autofill/core/browser/autofill_address_policy_handler.h"
#include "components/autofill/core/browser/autofill_credit_card_policy_handler.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmarks_policy_handler.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/enterprise/browser/reporting/cloud_reporting_policy_handler.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/driver/sync_policy_handler.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "ios/chrome/browser/policy/browser_signin_policy_handler.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using policy::PolicyToPreferenceMapEntry;
using policy::SimplePolicyHandler;

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
// clang-format off
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { policy::key::kChromeVariations,
    variations::prefs::kVariationsRestrictionsByPolicy,
    base::Value::Type::INTEGER },
  { policy::key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kEditBookmarksEnabled,
    bookmarks::prefs::kEditBookmarksEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kPasswordManagerEnabled,
    password_manager::prefs::kCredentialsEnableService,
    base::Value::Type::BOOLEAN },
  { policy::key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::Type::INTEGER },
  { policy::key::kIncognitoModeAvailability,
    prefs::kIncognitoModeAvailability,
    base::Value::Type::INTEGER },
  { policy::key::kNTPContentSuggestionsEnabled,
    prefs::kNTPContentSuggestionsEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kPolicyRefreshRate,
    policy::policy_prefs::kUserPolicyRefreshRate,
    base::Value::Type::INTEGER },
  { policy::key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::Type::LIST },
  { policy::key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::Type::LIST },
  { policy::key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kTranslateEnabled,
    translate::prefs::kOfferTranslateEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kURLAllowlist,
    policy::policy_prefs::kUrlAllowlist,
    base::Value::Type::LIST},
  { policy::key::kUrlKeyedAnonymizedDataCollectionEnabled,
    unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
    base::Value::Type::BOOLEAN },
  { policy::key::kRestrictAccountsToPatterns,
    prefs::kRestrictAccountsToPatterns,
    base::Value::Type::LIST },
};
// clang-format on

void PopulatePolicyHandlerParameters(
    policy::PolicyHandlerParameters* parameters) {}

}  // namespace

std::unique_ptr<policy::ConfigurationPolicyHandlerList> BuildPolicyHandlerList(
    bool allow_future_policies,
    const policy::Schema& chrome_schema) {
  DCHECK(IsEnterprisePolicyEnabled());
  std::unique_ptr<policy::ConfigurationPolicyHandlerList> handlers =
      std::make_unique<policy::ConfigurationPolicyHandlerList>(
          base::BindRepeating(&PopulatePolicyHandlerParameters),
          base::BindRepeating(&policy::GetChromePolicyDetails),
          allow_future_policies);

  // Check the feature flag before adding handlers to the list.
  if (!ShouldInstallEnterprisePolicyHandlers()) {
    return handlers;
  }

  for (size_t i = 0; i < base::size(kSimplePolicyMap); ++i) {
    handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
        kSimplePolicyMap[i].policy_name, kSimplePolicyMap[i].preference_path,
        kSimplePolicyMap[i].value_type));
  }

  handlers->AddHandler(
      std::make_unique<autofill::AutofillAddressPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<autofill::AutofillCreditCardPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<policy::BrowserSigninPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<policy::DefaultSearchPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<safe_browsing::SafeBrowsingPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<bookmarks::ManagedBookmarksPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<syncer::SyncPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<enterprise_reporting::CloudReportingPolicyHandler>());

  if (ShouldInstallURLBlocklistPolicyHandlers()) {
    handlers->AddHandler(std::make_unique<policy::URLBlocklistPolicyHandler>(
        policy::key::kURLBlocklist));
  }

  return handlers;
}
