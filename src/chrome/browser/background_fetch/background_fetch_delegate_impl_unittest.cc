// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const GURL kOriginUrl = GURL("https://example.com/");
const GURL kPageUrl = GURL("https://example.com/page1");
const char kUserInitiatedAbort[] = "UserInitiatedAbort";

}  // namespace

class BackgroundFetchDelegateImplTest : public testing::Test {
 public:
  void SetUp() override {
    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    delegate_ = static_cast<BackgroundFetchDelegateImpl*>(
        profile_.GetBackgroundFetchDelegate());
  }

  BackgroundFetchDelegateImpl* delegate() { return delegate_; }

  void WaitForHistoryQueryToComplete() {
    base::RunLoop run_loop;
    delegate()->set_history_query_complete_closure_for_testing(
        run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  // This is used to specify the main thread type of the tests as the UI
  // thread.
  content::TestBrowserThreadBundle threads_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;
  BackgroundFetchDelegateImpl* delegate_;
  TestingProfile profile_;
};

TEST_F(BackgroundFetchDelegateImplTest, HistoryServiceIntegration) {
  ASSERT_TRUE(profile_.CreateHistoryService(/* delete_file= */ true,
                                            /* no_db= */ false));
  auto* history_service = HistoryServiceFactory::GetForProfile(
      &profile_, ServiceAccessType::EXPLICIT_ACCESS);

  bool user_initiated_abort = true;

  url::Origin origin = url::Origin::Create(kOriginUrl);
  size_t initial_entries_count = recorder_->entries_count();
  size_t expected_entries_count = initial_entries_count + 1;

  // First, attempt to record an event for |origin| before it's been added to
  // the |history_service|. Nothing should be recorded.
  delegate()->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, user_initiated_abort);
  WaitForHistoryQueryToComplete();

  EXPECT_EQ(recorder_->entries_count(), initial_entries_count);

  // Now add |origin| to the |history_service|. After this, registration
  // deletion should cause UKM event to be logged.
  history_service->AddPage(kOriginUrl, base::Time::Now(),
                           history::SOURCE_BROWSED);
  delegate()->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, user_initiated_abort);
  WaitForHistoryQueryToComplete();

  EXPECT_EQ(recorder_->entries_count(), expected_entries_count);

  // Delete the |origin| from the |history_service|. Subsequent events should
  // not be logged to UKM anymore.
  history_service->DeleteURL(kOriginUrl);
  delegate()->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, user_initiated_abort);
  WaitForHistoryQueryToComplete();

  EXPECT_EQ(recorder_->entries_count(), expected_entries_count);
}

TEST_F(BackgroundFetchDelegateImplTest, HistoryServiceIntegrationUrlVsOrigin) {
  ASSERT_TRUE(profile_.CreateHistoryService(/* delete_file= */ true,
                                            /* no_db= */ false));
  auto* history_service = HistoryServiceFactory::GetForProfile(
      &profile_, ServiceAccessType::EXPLICIT_ACCESS);

  bool user_initiated_abort = true;

  url::Origin origin = url::Origin::Create(kOriginUrl);
  size_t initial_entries_count = recorder_->entries_count();
  size_t expected_entries_count = initial_entries_count + 1;

  // First, attempt to record an event for |origin| before it's been added to
  // the |history_service|. Nothing should be recorded.
  delegate()->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, user_initiated_abort);
  WaitForHistoryQueryToComplete();

  EXPECT_EQ(recorder_->entries_count(), initial_entries_count);

  // Now add |kPageUrl| to the |history_service|. After this, registration
  // deletion shouldn't cause UKM event to be logged for |origin|.
  history_service->AddPage(kPageUrl, base::Time::Now(),
                           history::SOURCE_BROWSED);
  delegate()->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, user_initiated_abort);
  WaitForHistoryQueryToComplete();

  EXPECT_EQ(recorder_->entries_count(), expected_entries_count);

  // Delete the |kPageUrl| from the |history_service|. Subsequent events should
  // not be logged to UKM anymore.
  history_service->DeleteURL(kPageUrl);
  delegate()->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, user_initiated_abort);
  WaitForHistoryQueryToComplete();

  EXPECT_EQ(recorder_->entries_count(), expected_entries_count);
}

TEST_F(BackgroundFetchDelegateImplTest, RecordUkmEvent) {
  url::Origin origin = url::Origin::Create(kOriginUrl);

  // Origin not found in the history service, nothing is recorded.
  delegate()->DidQueryUrl(kOriginUrl, /* user_initiated_abort= */ true,
                          /* success= */ false, /* num_visits= */ 0,
                          base::Time::Now());
  {
    std::vector<const ukm::mojom::UkmEntry*> entries =
        recorder_->GetEntriesByName(
            ukm::builders::BackgroundFetchDeletingRegistration::kEntryName);
    EXPECT_EQ(entries.size(), 0u);
  }

  // History Service finds the origin and the UKM event is logged.
  delegate()->DidQueryUrl(kOriginUrl, /* user_initiated_abort= */ true,
                          /* success= */ true, /* num_visits= */ 2,
                          base::Time::Now());
  {
    std::vector<const ukm::mojom::UkmEntry*> entries =
        recorder_->GetEntriesByName(
            ukm::builders::BackgroundFetchDeletingRegistration::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    auto* entry = recorder_->GetEntriesByName(
        ukm::builders::BackgroundFetchDeletingRegistration::kEntryName)[0];
    recorder_->ExpectEntryMetric(entry, kUserInitiatedAbort, 1);
  }
}
