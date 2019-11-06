// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileActivityMetricsRecorderTest : public testing::Test {
 public:
  ProfileActivityMetricsRecorderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());

    ProfileActivityMetricsRecorder::Initialize();
  }

  void ActivateBrowser(Profile* profile) {
    Browser::CreateParams browser_params(profile, false);
    TestBrowserWindow browser_window;
    browser_params.window = &browser_window;
    std::unique_ptr<Browser> browser(Browser::Create(browser_params));

    BrowserList::SetLastActive(browser.get());
  }

  void ActivateIncognitoBrowser(Profile* profile) {
    ActivateBrowser(profile->GetOffTheRecordProfile());
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  base::HistogramTester* histograms() { return &histogram_tester_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  TestingProfileManager profile_manager_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ProfileActivityMetricsRecorderTest);
};

TEST_F(ProfileActivityMetricsRecorderTest, GuestProfile) {
  Profile* regular_profile = profile_manager()->CreateTestingProfile("p1");
  Profile* guest_profile = profile_manager()->CreateGuestProfile();
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 0);

  // Check whether the regular profile is counted in bucket 2. (Bucket 1 is
  // reserved for the guest profile.)
  ActivateBrowser(regular_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);

  // Activate an incognito browser instance of the guest profile.
  // Note: Creating a non-incognito guest browser instance is not possible.
  ActivateIncognitoBrowser(guest_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/0, /*count=*/1);

  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 2);
}

TEST_F(ProfileActivityMetricsRecorderTest, IncognitoProfile) {
  Profile* regular_profile = profile_manager()->CreateTestingProfile("p1");
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 0);

  ActivateBrowser(regular_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);

  ActivateIncognitoBrowser(regular_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/2);

  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 2);
}

TEST_F(ProfileActivityMetricsRecorderTest, MultipleProfiles) {
  Profile* profile1 = profile_manager()->CreateTestingProfile("p1");
  Profile* profile2 = profile_manager()->CreateTestingProfile("p2");
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 0);

  // The browser of profile 1 is activated for the first time. It is assigned
  // bucket 1.
  ActivateBrowser(profile1);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);

  // Profile creation doesn't influence the histogram.
  Profile* profile3 = profile_manager()->CreateTestingProfile("p3");
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 1);

  ActivateBrowser(profile1);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/2);

  // The browser of profile 3 is activated for the first time. It is assigned
  // bucket 2.
  ActivateBrowser(profile3);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/2, /*count=*/1);

  // The browser of profile 2 is activated for the first time. It is assigned
  // bucket 3.
  ActivateBrowser(profile2);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/3, /*count=*/1);

  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 4);
}
