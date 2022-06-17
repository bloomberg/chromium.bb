// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#import "components/metrics/metrics_reporting_default_state.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/web/public/thread/web_thread.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kChromeFirstRunUIWillFinishNotification =
    @"kChromeFirstRunUIWillFinishNotification";

NSString* const kChromeFirstRunUIDidFinishNotification =
    @"kChromeFirstRunUIDidFinishNotification";

constexpr BOOL kDefaultMetricsReportingCheckboxValue = YES;

namespace {

// Trampoline method for Bind to create the sentinel file.
void CreateSentinel() {
  base::File::Error file_error;
  FirstRun::SentinelResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  base::UmaHistogramEnumeration("FirstRun.Sentinel.Created", sentinel_created,
                                FirstRun::SentinelResult::SENTINEL_RESULT_MAX);
  if (sentinel_created ==
      FirstRun::SentinelResult::SENTINEL_RESULT_FILE_ERROR) {
    base::UmaHistogramExactLinear("FirstRun.Sentinel.CreatedFileError",
                                  -file_error, -base::File::FILE_ERROR_MAX);
  }
}

bool kFirstRunSentinelCreated = false;

// Creates the First Run sentinel file so that the user will not be shown First
// Run on subsequent cold starts. The user is considered done with First Run
// only after a successful sign-in or explicitly skipping signing in. First Run
// metrics are recorded iff the sentinel file didn't previous exist and was
// successfully created.
void WriteFirstRunSentinelAndRecordMetrics(
    ChromeBrowserState* browserState,
    first_run::SignInAttemptStatus sign_in_attempt_status,
    BOOL has_sso_account) {
  DCHECK(!base::FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  kFirstRunSentinelCreated = true;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateSentinel));
  RecordFirstRunSignInMetrics(
      IdentityManagerFactory::GetForBrowserState(browserState),
      sign_in_attempt_status, has_sso_account);
}

}  // namespace

void RecordFirstRunSignInMetrics(
    signin::IdentityManager* identity_manager,
    first_run::SignInAttemptStatus sign_in_attempt_status,
    BOOL has_sso_accounts) {
  bool user_signed_in =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  first_run::SignInStatus sign_in_status;
  if (user_signed_in) {
    sign_in_status = has_sso_accounts
                         ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SUCCESSFUL
                         : first_run::SIGNIN_SUCCESSFUL;
  } else {
    switch (sign_in_attempt_status) {
      case first_run::SignInAttemptStatus::NOT_ATTEMPTED:
        sign_in_status = has_sso_accounts
                             ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_QUICK
                             : first_run::SIGNIN_SKIPPED_QUICK;
        break;
      case first_run::SignInAttemptStatus::ATTEMPTED:
        sign_in_status = has_sso_accounts
                             ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_GIVEUP
                             : first_run::SIGNIN_SKIPPED_GIVEUP;
        break;
      case first_run::SignInAttemptStatus::SKIPPED_BY_POLICY:
        sign_in_status = first_run::SIGNIN_SKIPPED_POLICY;
        break;
      case first_run::SignInAttemptStatus::NOT_SUPPORTED:
        sign_in_status = first_run::SIGNIN_NOT_SUPPORTED;
        break;
    }
  }
  base::UmaHistogramEnumeration("FirstRun.SignIn", sign_in_status,
                                first_run::SIGNIN_SIZE);
}

void RecordFirstRunScrollButtonVisibilityMetrics(
    first_run::FirstRunScreenType screen_type,
    BOOL scroll_button_visible) {
  switch (screen_type) {
    case first_run::FirstRunScreenType::kDefaultBrowserPromoScreen:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.DefaultBrowserPromoScreen",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kSignInScreenWithFooter:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.SignInScreenWithFooter",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::
        kSignInScreenWithFooterAndIdentityPicker:
      base::UmaHistogramBoolean("IOS.FirstRun.ScrollButtonVisible."
                                "SignInScreenWithFooterAndIdentityPicker",
                                scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kSignInScreenWithIdentityPicker:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.SignInScreenWithIdentityPicker",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::
        kSignInScreenWithoutFooterOrIdentityPicker:
      base::UmaHistogramBoolean("IOS.FirstRun.ScrollButtonVisible."
                                "SignInScreenWithoutFooterOrIdentityPicker",
                                scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kSyncScreenWithoutIdentityPicker:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.SyncScreenWithoutIdentityPicker",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kSyncScreenWithIdentityPicker:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.SyncScreenWithIdentityPicker",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kWelcomeScreenWithoutUMACheckbox:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.WelcomeScreenWithoutUMACheckbox",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kWelcomeScreenWithUMACheckbox:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.WelcomeScreenWithUMACheckbox",
          scroll_button_visible);
      break;
  }
}

void FinishFirstRun(ChromeBrowserState* browserState,
                    web::WebState* web_state,
                    FirstRunConfiguration* config,
                    id<SyncPresenter> presenter) {
  // This method souldn't be called with the new FRE, and should be removed
  // after the new FRE module is shipped.
  DCHECK(!base::FeatureList::IsEnabled(kEnableFREUIModuleIOS));

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIWillFinishNotification
                    object:nil];
  WriteFirstRunSentinelAndRecordMetrics(
      browserState, config.signInAttemptStatus, config.hasSSOAccount);

  // Display the sync errors infobar.
  DisplaySyncErrors(browserState, web_state, presenter);
}

void WriteFirstRunSentinel() {
  kFirstRunSentinelCreated = true;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateSentinel));
}

void FirstRunDismissed() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIDidFinishNotification
                    object:nil];
}

bool ShouldPresentFirstRunExperience() {
  if (experimental_flags::AlwaysDisplayFirstRun())
    return true;

  if (tests_hook::DisableFirstRun())
    return false;

  if (kFirstRunSentinelCreated)
    return false;

  return FirstRun::IsChromeFirstRun();
}

void RecordMetricsReportingDefaultState() {
  // Record metrics reporting as opt-in/opt-out only once.
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // Don't call RecordMetricsReportingDefaultState twice. This can happen if
    // the app is quit before accepting the TOS, or via experiment settings.
    if (metrics::GetMetricsReportingDefaultState(
            GetApplicationContext()->GetLocalState()) !=
        metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
      return;
    }

    metrics::RecordMetricsReportingDefaultState(
        GetApplicationContext()->GetLocalState(),
        kDefaultMetricsReportingCheckboxValue
            ? metrics::EnableMetricsDefault::OPT_OUT
            : metrics::EnableMetricsDefault::OPT_IN);
  });
}

bool IsApplicationManaged() {
  return [[[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] count] > 0;
}
