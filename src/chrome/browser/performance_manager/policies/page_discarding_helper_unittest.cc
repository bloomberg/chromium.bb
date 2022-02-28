// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

using ::testing::Return;

class PageDiscardingHelperTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  PageDiscardingHelperTest() = default;
  ~PageDiscardingHelperTest() override = default;
  PageDiscardingHelperTest(const PageDiscardingHelperTest& other) = delete;
  PageDiscardingHelperTest& operator=(const PageDiscardingHelperTest&) = delete;

  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_.reset();
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

 protected:
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PageDiscardingHelperTest, TestCannotDiscardVisiblePage) {
  page_node()->SetIsVisible(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardAudiblePage) {
  page_node()->SetIsAudible(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardPageWithDiscardAttemptMarker) {
  PageDiscardingHelper::GetFromGraph(graph())
      ->AddDiscardAttemptMarkerForTesting(page_node());
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardRecentlyAudiblePage) {
  page_node()->SetIsAudible(true);
  page_node()->SetIsAudible(false);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

#if !defined(OS_CHROMEOS)
TEST_F(PageDiscardingHelperTest, TestCannotDiscardRecentlyVisiblePage) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}
#endif

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPdf) {
  page_node()->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 53,
                                              GURL("https://foo.com/doc.pdf"),
                                              "application/pdf");
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithoutMainFrame) {
  ResetFrameNode();
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardExtension) {
  frame_node()->OnNavigationCommitted(GURL("chrome-extention://foo"), false);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithInvalidURL) {
  frame_node()->OnNavigationCommitted(GURL("foo42"), false);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageProtectedByExtension) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsAutoDiscardableForTesting(false);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageBeingMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest,
       TestCannotDiscardPageConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

TEST_F(PageDiscardingHelperTest, TestCannotDiscardIsConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

#if !defined(OS_CHROMEOS)
TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageMultipleTimes) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetWasDiscardedForTesting(true);
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}
#endif

TEST_F(PageDiscardingHelperTest, TestCannotDiscardPageWithFormInteractions) {
  frame_node()->SetHadFormInteraction();
  EXPECT_FALSE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
}

// Tests UrgentlyDiscardMultiplePages.

TEST_F(PageDiscardingHelperTest, UrgentlyDiscardMultiplePagesNoCandidate) {
  page_node()->SetIsVisible(true);

  // When discard_protected_tabs is false, protected page can not be discarded.
  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 1024, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ false,
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, UrgentlyDiscardMultiplePagesDiscardProtected) {
  page_node()->SetIsVisible(true);

  // When discard_protected_tabs is true, protected page can be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 1024, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ true,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, UrgentlyDiscardMultiplePagesTwoCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  EXPECT_TRUE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node2.get()));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // 2 candidates should both be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 2048, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ true,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest,
       UrgentlyDiscardMultiplePagesTwoCandidatesProtected) {
  // page_node() is audible and should not be discarded.
  page_node()->SetIsAudible(true);

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  EXPECT_TRUE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node2.get()));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // When discard_protected_tabs is false, it should not discard protected page
  // even with large reclaim_target_kb.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 1000000, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ false,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, UrgentlyDiscardMultiplePagesThreeCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  auto process_node3 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  main_frame_node3->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());

  page_node2->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  // |page_node3| is the most recently visible page.
  page_node3->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node3->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);
  process_node3->set_resident_set_kb(1024);

  // The 2 candidates with earlier last visible time should be discarded.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 1500, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ true,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 3,
                                        1);
}

TEST_F(PageDiscardingHelperTest,
       UrgentlyDiscardMultiplePagesThreeCandidatesWithPriority) {
  // page_node() is audible and should have lower discard priority.
  page_node()->SetIsAudible(true);

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  auto process_node3 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  main_frame_node3->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());

  page_node2->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  // |page_node3| is the most recently visible page.
  page_node3->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node3->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);
  process_node3->set_resident_set_kb(1024);

  // Protected pages should have lower discard priority.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node3.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 1500, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ true,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(PageDiscardingHelperTest, UrgentlyDiscardMultiplePagesNoDiscardable) {
  // UrgentlyDiscardMultiplePages should not retry indefinitely when all nodes
  // are not discardable.

  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(1024);

  // Discarding failed on all nodes.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(false));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ 10240, features::DiscardStrategy::LRU,
      /*discard_protected_tabs*/ true,
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

class ParameterizedPageDiscardingHelperTest
    : public PageDiscardingHelperTest,
      public ::testing::WithParamInterface<features::DiscardStrategy> {
 public:
  ParameterizedPageDiscardingHelperTest() = default;
  ~ParameterizedPageDiscardingHelperTest() override = default;
  ParameterizedPageDiscardingHelperTest(
      const ParameterizedPageDiscardingHelperTest& other) = delete;
  ParameterizedPageDiscardingHelperTest& operator=(
      const ParameterizedPageDiscardingHelperTest&) = delete;
};

// Tests UrgentlyDiscardAPage.

TEST_P(ParameterizedPageDiscardingHelperTest, UrgentlyDiscardAPageNoCandidate) {
  page_node()->SetIsVisible(true);
  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
      GetParam(), base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardAPageSingleCandidate) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
      GetParam(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);
}

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardAPageSingleCandidateFails) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
      GetParam(), base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // There should be 2 discard attempts, during the first one an attempt will be
  // made to discard |page_node()|, on the second attempt no discard candidate
  // should be found.
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);

  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 0,
                                        1);
}

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardAPageTwoCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Pretend that |page_node2| is the most recently visible page.
  page_node2->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));
  EXPECT_TRUE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node2.get()));
  EXPECT_GT(page_node()->TimeSinceLastVisibilityChange(),
            page_node2->TimeSinceLastVisibilityChange());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  if (GetParam() == features::DiscardStrategy::BIGGEST_RSS) {
    // |page_node2| should be discarded despite being the most recently visible
    // page as it has a bigger footprint.
    EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
        .WillOnce(Return(true));

    PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
        GetParam(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    ::testing::Mock::VerifyAndClearExpectations(discarder());
    histogram_tester()->ExpectUniqueSample("Discarding.LargestTabFootprint",
                                           2048 / 1024, 1);
    histogram_tester()->ExpectUniqueSample("Discarding.OldestTabFootprint",
                                           1024 / 1024, 1);
  } else {
    EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
        .WillOnce(Return(true));

    PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
        GetParam(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
    ::testing::Mock::VerifyAndClearExpectations(discarder());
  }
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 2,
                                        1);
}

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardAPageTwoCandidatesFirstFails) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // Pretends that the first discardable page hasn't been discarded
  // successfully, the other one should be discarded in this case.
  if (GetParam() == features::DiscardStrategy::BIGGEST_RSS) {
    ::testing::InSequence in_sequence;
    // The first candidate is the tab with the biggest RSS.
    EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
        .WillOnce(Return(false));
    EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
        .WillOnce(Return(true));
  } else {
    ::testing::InSequence in_sequence;
    // The first candidate is the least recently used tab.
    EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
        .WillOnce(Return(false));
    EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
        .WillOnce(Return(true));
  }

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
      GetParam(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardAPageTwoCandidatesMultipleFrames) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  // Adds a second frame to |page_node()| and host it in |process_node2|.
  auto page_node1_extra_frame =
      CreateFrameNodeAutoId(process_node2.get(), page_node(), frame_node());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // The total RSS of |page_node()| should be 1024 + 2048 / 2 = 2048 and the
  // RSS of |page_node2| should be 2048 / 2 = 1024, so |page_node()| will get
  // discarded despite having spent less time in background than |page_node2|.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
      GetParam(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardAPageTwoCandidatesNoRSSData) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Pretend that |page_node()| is the most recently visible page.
  page_node()->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));
  EXPECT_TRUE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
  EXPECT_GT(page_node2->TimeSinceLastVisibilityChange(),
            page_node()->TimeSinceLastVisibilityChange());

  // |page_node2| should be discarded as there's no RSS data for any of the
  // pages and it's the least recently visible page.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardAPage(
      GetParam(), base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

// Tests UrgentlyDiscardMultiplePages with reclaim_target_kb == nullopt.

TEST_P(ParameterizedPageDiscardingHelperTest,
       UrgentlyDiscardMultiplePagesTwoCandidatesNoRSSData) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());

  // Pretend that |page_node()| is the most recently visible page.
  page_node()->SetIsVisible(true);
  AdvanceClock(base::Minutes(30));
  page_node()->SetIsVisible(false);
  AdvanceClock(base::Minutes(30));
  EXPECT_TRUE(
      PageDiscardingHelper::GetFromGraph(graph())->CanUrgentlyDiscardForTesting(
          page_node()));
  EXPECT_GT(page_node2->TimeSinceLastVisibilityChange(),
            page_node()->TimeSinceLastVisibilityChange());

  // |page_node2| should be discarded as there's no RSS data for any of the
  // pages and it's the least recently visible page.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));

  PageDiscardingHelper::GetFromGraph(graph())->UrgentlyDiscardMultiplePages(
      /*reclaim_target_kb*/ absl::nullopt, GetParam(),
      /*discard_protected_tabs*/ true,
      base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

INSTANTIATE_TEST_CASE_P(
    PageDiscardingHelperWithParamTest,
    ParameterizedPageDiscardingHelperTest,
    ::testing::Values(features::DiscardStrategy::LRU,
                      features::DiscardStrategy::BIGGEST_RSS));

}  // namespace policies
}  // namespace performance_manager
