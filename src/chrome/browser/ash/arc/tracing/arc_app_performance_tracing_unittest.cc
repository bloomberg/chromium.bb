// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"

#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_test_helper.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_uma_session.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

constexpr char kFocusAppPackage[] = "focus.app.package";
constexpr char kFocusAppActivity[] = "focus.app.package.Activity";
constexpr char kFocusCategory[] = "OnlineGame";
constexpr char kNonFocusAppPackage[] = "nonfocus.app.package";
constexpr char kNonFocusAppActivity[] = "nonfocus.app.package.Activity";

// For 20 frames.
constexpr base::TimeDelta kTestPeriod = base::Seconds(1) / (60 / 20);

constexpr int kMillisecondsToFirstFrame = 500;

// Creates name of histogram with required statistics.
std::string GetFocusStatisticName(const std::string& name) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            kFocusCategory);
}

// Reads statistics value from histogram.
int64_t ReadFocusStatistics(const std::string& name) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(GetFocusStatisticName(name));
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotFinalDelta();
  DCHECK(samples.get());
  DCHECK_EQ(1, samples->TotalCount());
  return samples->sum();
}

}  // namespace

// BrowserWithTestWindowTest contains required ash/shell support that would not
// be possible to use directly.
class ArcAppPerformanceTracingTest : public BrowserWithTestWindowTest {
 public:
  ArcAppPerformanceTracingTest() = default;

  ArcAppPerformanceTracingTest(const ArcAppPerformanceTracingTest&) = delete;
  ArcAppPerformanceTracingTest& operator=(const ArcAppPerformanceTracingTest&) =
      delete;

  ~ArcAppPerformanceTracingTest() override = default;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    arc_test_.SetUp(profile());
    tracing_helper_.SetUp(profile());

    ArcAppPerformanceTracing::SetFocusAppForTesting(
        kFocusAppPackage, kFocusAppActivity, kFocusCategory);

    ArcAppPerformanceTracingUmaSession::SetTracingPeriodForTesting(kTestPeriod);
  }

  void TearDown() override {
    shell_root_surface_.reset();
    tracing_helper_.TearDown();
    arc_test_.TearDown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Ensures that tracing is active.
  views::Widget* StartArcFocusAppTracing() {
    shell_root_surface_ = std::make_unique<exo::Surface>();
    views::Widget* const arc_widget =
        ArcAppPerformanceTracingTestHelper::CreateArcWindow(
            "org.chromium.arc.1", shell_root_surface_.get());
    DCHECK(arc_widget && arc_widget->GetNativeWindow());
    tracing_helper().GetTracing()->OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
    tracing_helper().GetTracing()->OnTaskCreated(
        1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
        std::string() /* intent */, 0 /* session_id */);
    DCHECK(tracing_helper().GetTracingSession());
    tracing_helper().GetTracingSession()->FireTimerForTesting();
    DCHECK(tracing_helper().GetTracingSession());
    DCHECK(tracing_helper().GetTracingSession()->tracing_active());
    return arc_widget;
  }

  ArcAppPerformanceTracingTestHelper& tracing_helper() {
    return tracing_helper_;
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SyncServiceFactory::GetInstance(),
             SyncServiceFactory::GetDefaultFactory()},
            {ChromeSigninClientFactory::GetInstance(),
             base::BindRepeating(&signin::BuildTestSigninClient)}};
  }

 private:
  std::unique_ptr<exo::Surface> shell_root_surface_;
  ArcAppPerformanceTracingTestHelper tracing_helper_;
  ArcAppTest arc_test_;
};

TEST_F(ArcAppPerformanceTracingTest, TracingScheduled) {
  // By default it is inactive.
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  // Report task first.
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  // Create window second.
  exo::Surface shell_root_surface1;
  views::Widget* const arc_widget1 =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.1",
                                                          &shell_root_surface1);
  ASSERT_TRUE(arc_widget1);
  ASSERT_TRUE(arc_widget1->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget1->GetNativeWindow(), nullptr /* lost_active */);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  // Scheduled but not started.
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());

  // Test reverse order, create window first.
  exo::Surface shell_root_surface2;
  views::Widget* const arc_widget2 =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.2",
                                                          &shell_root_surface2);
  ASSERT_TRUE(arc_widget2);
  ASSERT_TRUE(arc_widget2->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget2->GetNativeWindow(), arc_widget2->GetNativeWindow());
  // Task is not yet created, this also resets previous tracing.
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  // Report task second.
  tracing_helper().GetTracing()->OnTaskCreated(
      2 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  // Scheduled but not started.
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  arc_widget1->Close();
  arc_widget2->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingNotScheduledForNonFocusApp) {
  exo::Surface shell_root_surface;
  views::Widget* const arc_widget =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.1",
                                                          &shell_root_surface);
  ASSERT_TRUE(arc_widget);
  ASSERT_TRUE(arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kNonFocusAppPackage, kNonFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingStoppedOnIdle) {
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  const base::TimeDelta normal_interval = base::Seconds(1) / 60;
  base::Time timestamp = base::Time::Now();
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  // Expected updates;
  timestamp += normal_interval;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());

  timestamp += normal_interval * 5;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());

  // Too long update.
  timestamp += normal_interval * 10;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  // Tracing is rescheduled and no longer active.
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, StatisticsReported) {
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  EXPECT_FALSE(tracing_helper().GetTracing()->WasReported(kFocusCategory));
  tracing_helper().PlayDefaultSequence();
  tracing_helper().FireTimerForTesting();
  EXPECT_TRUE(tracing_helper().GetTracing()->WasReported(kFocusCategory));
  EXPECT_EQ(45L, ReadFocusStatistics("FPS2"));
  EXPECT_EQ(216L, ReadFocusStatistics("CommitDeviation2"));
  EXPECT_EQ(48L, ReadFocusStatistics("RenderQuality2"));
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingNotScheduledWhenAppSyncDisabled) {
  tracing_helper().DisableAppSync();
  exo::Surface shell_root_surface;
  views::Widget* const arc_widget =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.1",
                                                          &shell_root_surface);
  ASSERT_TRUE(arc_widget);
  ASSERT_TRUE(arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TimeToFirstFrameRendered) {
  const std::string app_id =
      ArcAppListPrefs::GetAppId(kFocusAppPackage, kFocusAppActivity);
  exo::Surface shell_root_surface;
  views::Widget* const arc_widget =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.1",
                                                          &shell_root_surface);
  DCHECK(arc_widget && arc_widget->GetNativeWindow());

  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */, 0 /* session_id */);

  // No report before launch
  base::Time timestamp = base::Time::Now();
  tracing_helper().GetTracing()->HandleActiveAppRendered(timestamp);
  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      "Arc.Runtime.Performance.Generic.FirstFrameRendered");
  DCHECK(!histogram);

  // Succesful report after launch
  ArcAppListPrefs::Get(profile())->SetLaunchRequestTimeForTesting(app_id,
                                                                  timestamp);
  timestamp += base::Milliseconds(kMillisecondsToFirstFrame);
  tracing_helper().GetTracing()->HandleActiveAppRendered(timestamp);
  histogram = base::StatisticsRecorder::FindHistogram(
      "Arc.Runtime.Performance.Generic.FirstFrameRendered");
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples = histogram->SnapshotDelta();
  DCHECK(samples.get());
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(kMillisecondsToFirstFrame, samples->sum());

  // No double report
  timestamp += base::Milliseconds(kMillisecondsToFirstFrame);
  tracing_helper().GetTracing()->HandleActiveAppRendered(timestamp);

  samples = histogram->SnapshotDelta();
  DCHECK(samples.get());
  EXPECT_EQ(0, samples->TotalCount());

  arc_widget->Close();
}

}  // namespace arc
