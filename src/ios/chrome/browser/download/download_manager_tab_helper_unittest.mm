// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/download_manager_tab_helper.h"

#include <memory>

#import "ios/chrome/test/fakes/fake_download_manager_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
char kUrl[] = "https://test.test/";
const char kMimeType[] = "";
}

// Test fixture for testing DownloadManagerTabHelper class.
class DownloadManagerTabHelperTest : public PlatformTest {
 protected:
  DownloadManagerTabHelperTest()
      : web_state_(std::make_unique<web::FakeWebState>()),
        delegate_([[FakeDownloadManagerTabHelperDelegate alloc] init]) {
    DownloadManagerTabHelper::CreateForWebState(web_state_.get(), delegate_);
  }

  DownloadManagerTabHelper* tab_helper() {
    return DownloadManagerTabHelper::FromWebState(web_state_.get());
  }

  std::unique_ptr<web::FakeWebState> web_state_;
  FakeDownloadManagerTabHelperDelegate* delegate_;
};

// Tests that created download has NotStarted state for visible web state.
TEST_F(DownloadManagerTabHelperTest, DownloadCreationForVisibleWebState) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);

  tab_helper()->Download(std::move(task));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests creating a second download after the first download is completed.
TEST_F(DownloadManagerTabHelperTest, DownloadAcceptationOnceCompleted) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  task->SetDone(true);
  tab_helper()->Download(std::move(task));
  EXPECT_EQ(web::DownloadTask::State::kComplete, *delegate_.state);

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  tab_helper()->Download(std::move(task2));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests creating the second download while the first download is still in
// progress. Second download will be rejected by the delegate.
TEST_F(DownloadManagerTabHelperTest, DownloadRejectionViaDelegate) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  tab_helper()->Download(std::move(task));

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  const web::FakeDownloadTask* task2_ptr = task2.get();
  tab_helper()->Download(std::move(task2));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(task2_ptr, delegate_.decidingPolicyForDownload);

  // Ask the delegate to discard the new download.
  BOOL discarded = [delegate_ decidePolicy:kNewDownloadPolicyDiscard];
  ASSERT_TRUE(discarded);
}

// Tests creating the second download while the first download is still in
// progress. Second download will be acccepted by the delegate.
TEST_F(DownloadManagerTabHelperTest, DownloadReplacingViaDelegate) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  tab_helper()->Download(std::move(task));

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  const web::FakeDownloadTask* task2_ptr = task2.get();
  tab_helper()->Download(std::move(task2));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(task2_ptr, delegate_.decidingPolicyForDownload);

  // Ask the delegate to replace the new download.
  BOOL replaced = [delegate_ decidePolicy:kNewDownloadPolicyReplace];
  ASSERT_TRUE(replaced);

  // The state of a new download task is kNotStarted.
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests that created download has null state for hidden web state.
TEST_F(DownloadManagerTabHelperTest, DownloadCreationForHiddenWebState) {
  web_state_->WasHidden();
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);

  tab_helper()->Download(std::move(task));

  ASSERT_FALSE(delegate_.state);
}

// Tests hiding and showing WebState.
TEST_F(DownloadManagerTabHelperTest, HideAndShowWebState) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);

  web_state_->WasHidden();
  EXPECT_FALSE(delegate_.state);

  web_state_->WasShown();
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests that has_download_task() returns correct result.
TEST_F(DownloadManagerTabHelperTest, HasDownloadTask) {
  ASSERT_FALSE(delegate_.state);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);

  web::FakeDownloadTask* task_ptr = task.get();
  ASSERT_FALSE(tab_helper()->has_download_task());
  tab_helper()->Download(std::move(task));
  task_ptr->Start(std::make_unique<net::URLFetcherStringWriter>());
  ASSERT_TRUE(tab_helper()->has_download_task());

  task_ptr->Cancel();
  EXPECT_FALSE(tab_helper()->has_download_task());
}

