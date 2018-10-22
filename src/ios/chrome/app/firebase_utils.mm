// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/firebase_utils.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/app/firebase_buildflags.h"
#include "ios/chrome/browser/application_context.h"
#if BUILDFLAG(FIREBASE_ENABLED)
#import "ios/third_party/firebase/Analytics/FirebaseCore.framework/Headers/FIRApp.h"
#endif  // BUILDFLAG(FIREBASE_ENABLED)

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kFirebaseConfiguredHistogramName[] =
    "FirstRun.IOSFirebaseConfigured";

const int kConversionAttributionWindowInDays = 90;

void InitializeFirebase(bool is_first_run) {
#if BUILDFLAG(FIREBASE_ENABLED)
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  const int64_t install_date = prefs->GetInt64(metrics::prefs::kInstallDate);
  base::TimeDelta installed_delta =
      base::TimeDelta::FromSeconds(base::Time::Now().ToTimeT() - install_date);
  // Initialize Firebase SDK to allow first_open event to be uploaded until
  // Attribution Window has passed.
  FirebaseConfiguredState enabled_state;
  if (installed_delta.InDaysFloored() >= kConversionAttributionWindowInDays) {
    enabled_state = FirebaseConfiguredState::kDisabledConversionWindow;
  } else {
    [FIRApp configure];
    enabled_state = is_first_run ? FirebaseConfiguredState::kEnabledFirstRun
                                 : FirebaseConfiguredState::kEnabledNotFirstRun;
  }
  UMA_HISTOGRAM_ENUMERATION(kFirebaseConfiguredHistogramName, enabled_state);
#else
  // FIRApp class should not exist if Firebase is not enabled.
  DCHECK_EQ(nil, NSClassFromString(@"FIRApp"));
  UMA_HISTOGRAM_ENUMERATION(kFirebaseConfiguredHistogramName,
                            FirebaseConfiguredState::kDisabled);
#endif  // BUILDFLAG(FIREBASE_ENABLED)
}
