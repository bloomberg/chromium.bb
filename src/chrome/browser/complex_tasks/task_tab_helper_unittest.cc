// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/complex_tasks/task_tab_helper.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockTaskTabHelper : public tasks::TaskTabHelper {
 public:
  MOCK_CONST_METHOD0(GetSpokeEntryHubType, HubType());

  static void CreateForWebContents(content::WebContents* contents) {
    DCHECK(contents);
    if (!FromWebContents(contents))
      contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new MockTaskTabHelper(contents)));
  }

  static MockTaskTabHelper* FromWebContents(content::WebContents* contents) {
    DCHECK(contents);
    return static_cast<MockTaskTabHelper*>(
        contents->GetUserData(UserDataKey()));
  }

  explicit MockTaskTabHelper(content::WebContents* web_contents)
      : tasks::TaskTabHelper(web_contents) {}

  friend class TaskTabHelperUnitTest;
};

class TaskTabHelperUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  const std::string kSearchDomain = "http://www.google.com/";
  const GURL kSearchURL = GURL(kSearchDomain);
  const std::string from_default_search_engine_histogram =
      "Tabs.Tasks.HubAndSpokeNavigationUsage.FromDefaultSearchEngine";
  const std::string from_form_submit_histogram =
      "Tabs.Tasks.HubAndSpokeNavigationUsage.FromFormSubmit";
  const std::string from_others_histogram =
      "Tabs.Tasks.HubAndSpokeNavigationUsage.FromOther";

  const MockTaskTabHelper::HubType DEFAULT_SEARCH_ENGINE_HUB_TYPE =
      MockTaskTabHelper::HubType::DEFAULT_SEARCH_ENGINE;
  const MockTaskTabHelper::HubType FORM_SUBMIT_HUB_TYPE =
      MockTaskTabHelper::HubType::FORM_SUBMIT;
  const MockTaskTabHelper::HubType OTHER_HUB_TYPE =
      MockTaskTabHelper::HubType::OTHER;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    MockTaskTabHelper::CreateForWebContents(web_contents());
    task_tab_helper_ = MockTaskTabHelper::FromWebContents(web_contents());
    NavigateAndCommit(kSearchURL);

    ON_CALL(*task_tab_helper_, GetSpokeEntryHubType())
        .WillByDefault(testing::Return(DEFAULT_SEARCH_ENGINE_HUB_TYPE));
  }

  void GoBackNTimes(int times) {
    while (times--) {
      content::NavigationSimulator::GoBack(web_contents());
    }
  }

  int GetSpokesCount(int entry_id) {
    return task_tab_helper_->GetSpokesForTesting(entry_id);
  }

  void NavigateAndCommitNTimes(int times) {
    while (times--) {
      static int unique_int = 0;
      // Note: The URLs need to be different on each iteration. Otherwise,
      // navigations will be treated as reloads and will not create a new
      // NavigationEntry.
      NavigateAndCommit(
          GURL(kSearchDomain + base::NumberToString(++unique_int)));
    }
  }

  MockTaskTabHelper* task_tab_helper_;
  base::HistogramTester histogram_tester_;
};

// Testing the reset counter logic
TEST_F(TaskTabHelperUnitTest, SpokeCountShouldResetInNavigationEntryCommitted) {
  NavigateAndCommitNTimes(2);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);
  EXPECT_EQ(GetSpokesCount(1), 2);

  GoBackNTimes(2);
  NavigateAndCommitNTimes(1);
  EXPECT_EQ(GetSpokesCount(1), 1);
}

TEST_F(TaskTabHelperUnitTest,
       SpokeCountShouldNotResetInNavigationEntryCommitted) {
  NavigateAndCommitNTimes(2);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);
  EXPECT_EQ(GetSpokesCount(1), 2);

  NavigateAndCommitNTimes(2);
  EXPECT_EQ(GetSpokesCount(1), 2);
}

// Testing the recording
TEST_F(TaskTabHelperUnitTest,
       SimpleRecordHubAndSpokeUsageFromDefaultSearchEngine) {
  EXPECT_CALL(*task_tab_helper_, GetSpokeEntryHubType())
      .WillOnce(testing::Return(DEFAULT_SEARCH_ENGINE_HUB_TYPE));

  NavigateAndCommitNTimes(1);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);

  histogram_tester_.ExpectBucketCount(from_default_search_engine_histogram, 2,
                                      1);
}

TEST_F(TaskTabHelperUnitTest, SimpleRecordHubAndSpokeUsageFromFormSubmit) {
  EXPECT_CALL(*task_tab_helper_, GetSpokeEntryHubType())
      .WillOnce(testing::Return(FORM_SUBMIT_HUB_TYPE));

  NavigateAndCommitNTimes(1);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);

  histogram_tester_.ExpectBucketCount(from_form_submit_histogram, 2, 1);
}

TEST_F(TaskTabHelperUnitTest, SimpleRecordHubAndSpokeUsageFromOther) {
  EXPECT_CALL(*task_tab_helper_, GetSpokeEntryHubType())
      .WillOnce(testing::Return(OTHER_HUB_TYPE));

  NavigateAndCommitNTimes(1);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);

  histogram_tester_.ExpectBucketCount(from_others_histogram, 2, 1);
}

TEST_F(TaskTabHelperUnitTest, ComplexRecordHubAndSpokeUsage) {
  {
    testing::InSequence s;

    EXPECT_CALL(*task_tab_helper_, GetSpokeEntryHubType())
        .Times(2)
        .WillRepeatedly(testing::Return(DEFAULT_SEARCH_ENGINE_HUB_TYPE))
        .RetiresOnSaturation();

    EXPECT_CALL(*task_tab_helper_, GetSpokeEntryHubType())
        .WillOnce(testing::Return(FORM_SUBMIT_HUB_TYPE));
  }

  NavigateAndCommitNTimes(1);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(2);
  GoBackNTimes(1);
  NavigateAndCommitNTimes(1);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(from_default_search_engine_histogram),
      testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(from_form_submit_histogram),
              testing::ElementsAre(base::Bucket(2, 1)));
}
