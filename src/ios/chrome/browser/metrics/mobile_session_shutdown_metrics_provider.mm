// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/metrics/metrics_service.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Logs |type| in the shutdown type histogram.
void LogShutdownType(MobileSessionShutdownType type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.MobileSessionShutdownType",
                                      type, MOBILE_SESSION_SHUTDOWN_TYPE_COUNT);
}

}  // namespace

MobileSessionShutdownMetricsProvider::MobileSessionShutdownMetricsProvider(
    metrics::MetricsService* metrics_service)
    : metrics_service_(metrics_service) {
  DCHECK(metrics_service_);
}

MobileSessionShutdownMetricsProvider::~MobileSessionShutdownMetricsProvider() {}

bool MobileSessionShutdownMetricsProvider::HasPreviousSessionData() {
  return true;
}

void MobileSessionShutdownMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // If app was upgraded since the last session, even if the previous session
  // ended in an unclean shutdown (crash, may or may not be UTE), this should
  // *not* be logged into one of the Foreground* or Background* states of
  // MobileSessionShutdownType. The crash is really from the pre-upgraded
  // version of app. Logging it now will incorrectly inflate the current
  // version's crash count with a crash that happened in a previous version of
  // the app.
  //
  // Not counting first run after upgrade does *not* bias the distribution of
  // the 4 Foreground* termination states because the reason of a crash would
  // not be affected by an imminent upgrade of Chrome app. Thus, the ratio of
  // Foreground shutdowns w/ crash log vs. w/o crash log is expected to be the
  // same regardless of whether First Launch after Upgrade is considered or not.
  if (IsFirstLaunchAfterUpgrade()) {
    LogShutdownType(FIRST_LAUNCH_AFTER_UPGRADE);
    return;
  }

  // If the last app lifetime did not end with a crash, then log it as a normal
  // shutdown while in the background.
  if (metrics_service_->WasLastShutdownClean()) {
    LogShutdownType(SHUTDOWN_IN_BACKGROUND);
    return;
  }

  // If the last app lifetime ended with main thread not responding, log it as
  // main thread frozen shutdown.
  if (LastSessionEndedFrozen()) {
    LogShutdownType(SHUTDOWN_IN_FOREGROUND_WITH_MAIN_THREAD_FROZEN);
    return;
  }

  // If the last app lifetime ended in a crash, log the type of crash.
  MobileSessionShutdownType shutdown_type;
  if (ReceivedMemoryWarningBeforeLastShutdown()) {
    if (HasCrashLogs()) {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING;
    } else {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING;
    }
  } else {
    if (HasCrashLogs()) {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING;
    } else {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING;
    }
  }
  LogShutdownType(shutdown_type);
}

bool MobileSessionShutdownMetricsProvider::IsFirstLaunchAfterUpgrade() {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

bool MobileSessionShutdownMetricsProvider::HasCrashLogs() {
  return breakpad_helper::HasReportToUpload();
}

bool MobileSessionShutdownMetricsProvider::LastSessionEndedFrozen() {
  return [MainThreadFreezeDetector sharedInstance].lastSessionEndedFrozen;
}

bool MobileSessionShutdownMetricsProvider::
    ReceivedMemoryWarningBeforeLastShutdown() {
  return [[PreviousSessionInfo sharedInstance]
      didSeeMemoryWarningShortlyBeforeTerminating];
}
