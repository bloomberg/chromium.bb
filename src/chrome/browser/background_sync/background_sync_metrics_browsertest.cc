// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_metrics.h"

#include <memory>

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"
#include "url/origin.h"

class BackgroundSyncMetricsBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundSyncMetricsBrowserTest() = default;
  ~BackgroundSyncMetricsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();

    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    auto* history_service = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    DCHECK(history_service);

    background_sync_metrics_ =
        std::make_unique<BackgroundSyncMetrics>(history_service);

    // Adds the URL to the history so that UKM events for this origin are
    // recorded.
    history_service->AddPage(GURL("https://backgroundsync.com/page"),
                             base::Time::Now(), history::SOURCE_BROWSED);
  }

 protected:
  void WaitForUkm() {
    base::RunLoop run_loop;
    background_sync_metrics_->ukm_event_recorded_for_testing_ =
        run_loop.QuitClosure();
    run_loop.Run();
  }

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;
  std::unique_ptr<BackgroundSyncMetrics> background_sync_metrics_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BackgroundSyncMetricsBrowserTest,
                       NoUkmRecordingsWithMissingHistory) {
  background_sync_metrics_->MaybeRecordRegistrationEvent(
      url::Origin::Create(GURL("https://notinhistory.com")),
      /* can_fire= */ false,
      /* is_reregistered= */ false);

  background_sync_metrics_->MaybeRecordRegistrationEvent(
      url::Origin::Create(GURL("https://backgroundsync.com")),
      /* can_fire= */ true,
      /* is_reregistered= */ false);
  WaitForUkm();

  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::BackgroundSyncRegistered::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    const auto* entry = entries[0];
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncRegistered::kCanFireName, true);
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncRegistered::kIsReregisteredName,
        false);
  }

  background_sync_metrics_->MaybeRecordCompletionEvent(
      url::Origin::Create(GURL("https://backgroundsync.com")),
      /* status_code= */ blink::ServiceWorkerStatusCode::kOk,
      /* num_attempts= */ 2, /* max_attempts= */ 5);
  WaitForUkm();

  // Sanity check that no additional BackgroundSyncRegistered events were
  // logged.
  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::BackgroundSyncRegistered::kEntryName);
    EXPECT_EQ(entries.size(), 1u);
  }

  {
    auto entries = recorder_->GetEntriesByName(
        ukm::builders::BackgroundSyncCompleted::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    const auto* entry = entries[0];
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncCompleted::kStatusName,
        static_cast<int64_t>(blink::ServiceWorkerStatusCode::kOk));
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncCompleted::kNumAttemptsName, 2);
    recorder_->ExpectEntryMetric(
        entry, ukm::builders::BackgroundSyncCompleted::kMaxAttemptsName, 5);
  }
}
