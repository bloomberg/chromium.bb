// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace {

// The maximum number of profiles that are recorded. This means that all
// profiles with bucket index greater than |kMaxProfileBucket| won't be included
// in the metrics.
constexpr int kMaxProfileBucket = 100;

int GetMetricsBucketIndex(Profile* profile) {
  if (profile->IsGuestSession())
    return 0;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    // This can happen if the profile is deleted.
    VLOG(1) << "Failed to read profile bucket index because attributes entry "
               "doesn't exist.";
    return -1;
  }
  return entry->GetMetricsBucketIndex();
}

}  // namespace

// static
void ProfileActivityMetricsRecorder::Initialize() {
  static base::NoDestructor<ProfileActivityMetricsRecorder>
      s_profile_activity_metrics_recorder;
}

ProfileActivityMetricsRecorder::ProfileActivityMetricsRecorder() {
  BrowserList::AddObserver(this);
}

void ProfileActivityMetricsRecorder::OnBrowserSetLastActive(Browser* browser) {
  int profile_bucket =
      GetMetricsBucketIndex(browser->profile()->GetOriginalProfile());

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    UMA_HISTOGRAM_EXACT_LINEAR("Profile.BrowserActive.PerProfile",
                               profile_bucket, kMaxProfileBucket);
  }
}
