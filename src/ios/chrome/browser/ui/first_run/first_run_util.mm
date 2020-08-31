// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
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

namespace {

// Trampoline method for Bind to create the sentinel file.
void CreateSentinel() {
  base::File::Error file_error;
  FirstRun::SentinelResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  UMA_HISTOGRAM_ENUMERATION("FirstRun.Sentinel.Created", sentinel_created,
                            FirstRun::SentinelResult::SENTINEL_RESULT_MAX);
  if (sentinel_created == FirstRun::SentinelResult::SENTINEL_RESULT_FILE_ERROR)
    UMA_HISTOGRAM_ENUMERATION("FirstRun.Sentinel.CreatedFileError", -file_error,
                              -base::File::FILE_ERROR_MAX);
}

// Helper function for recording first run metrics.
void RecordFirstRunMetricsInternal(ChromeBrowserState* browserState,
                                   bool sign_in_attempted,
                                   bool has_sso_accounts) {
  first_run::SignInStatus sign_in_status;
  bool user_signed_in = IdentityManagerFactory::GetForBrowserState(browserState)
                            ->HasPrimaryAccount();
  if (user_signed_in) {
    sign_in_status = has_sso_accounts
                         ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SUCCESSFUL
                         : first_run::SIGNIN_SUCCESSFUL;
  } else {
    if (sign_in_attempted) {
      sign_in_status = has_sso_accounts
                           ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_GIVEUP
                           : first_run::SIGNIN_SKIPPED_GIVEUP;
    } else {
      sign_in_status = has_sso_accounts
                           ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_QUICK
                           : first_run::SIGNIN_SKIPPED_QUICK;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("FirstRun.SignIn", sign_in_status,
                            first_run::SIGNIN_SIZE);
}

}  // namespace

void WriteFirstRunSentinelAndRecordMetrics(ChromeBrowserState* browserState,
                                           BOOL sign_in_attempted,
                                           BOOL has_sso_account) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateSentinel));
  RecordFirstRunMetricsInternal(browserState, sign_in_attempted,
                                has_sso_account);
}

void FinishFirstRun(ChromeBrowserState* browserState,
                    web::WebState* web_state,
                    FirstRunConfiguration* config,
                    id<SyncPresenter> presenter) {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIWillFinishNotification
                    object:nil];
  WriteFirstRunSentinelAndRecordMetrics(browserState, config.signInAttempted,
                                        config.hasSSOAccount);

  // Display the sync errors infobar.
  DisplaySyncErrors(browserState, web_state, presenter);
}

void FirstRunDismissed() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIDidFinishNotification
                    object:nil];
}
