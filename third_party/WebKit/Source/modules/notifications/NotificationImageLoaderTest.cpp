// Copyright 2016 Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationImageLoader.h"

#include "core/dom/ExecutionContext.h"
#include "core/testing/PageTestBase.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupportWithMockScheduler.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Functional.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {
namespace {

enum class NotificationLoadState { kNotLoaded, kLoadFailed, kLoadSuccessful };

constexpr char kNotificationImageLoaderBaseUrl[] = "http://test.com/";
constexpr char kNotificationImageLoaderBaseDir[] = "notifications/";
constexpr char kNotificationImageLoaderIcon500x500[] = "500x500.png";

// This mirrors the definition in NotificationImageLoader.cpp.
constexpr unsigned long kImageFetchTimeoutInMs = 90000;

static_assert(kImageFetchTimeoutInMs > 1000.0,
              "kImageFetchTimeoutInMs must be greater than 1000ms.");

class NotificationImageLoaderTest : public PageTestBase {
 public:
  NotificationImageLoaderTest()
      :  // Use an arbitrary type, since it only affects which UMA bucket we
         // use.
        loader_(
            new NotificationImageLoader(NotificationImageLoader::Type::kIcon)) {
  }

  ~NotificationImageLoaderTest() override {
    loader_->Stop();
    platform_->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetUp() override { PageTestBase::SetUp(IntSize()); }

  // Registers a mocked URL. When fetched it will be loaded form the test data
  // directory.
  WebURL RegisterMockedURL(const String& file_name) {
    WebURL registered_url = URLTestHelpers::RegisterMockedURLLoadFromBase(
        kNotificationImageLoaderBaseUrl,
        testing::CoreTestDataPath(kNotificationImageLoaderBaseDir), file_name,
        "image/png");
    return registered_url;
  }

  // Callback for the NotificationImageLoader. This will set the state of the
  // load as either success or failed based on whether the bitmap is empty.
  void ImageLoaded(const SkBitmap& bitmap) {
    if (!bitmap.empty())
      loaded_ = NotificationLoadState::kLoadSuccessful;
    else
      loaded_ = NotificationLoadState::kLoadFailed;
  }

  void LoadImage(const KURL& url) {
    loader_->Start(
        Context(), url,
        Bind(&NotificationImageLoaderTest::ImageLoaded, WTF::Unretained(this)));
  }

  ExecutionContext* Context() const { return &GetDocument(); }
  NotificationLoadState Loaded() const { return loaded_; }

 protected:
  HistogramTester histogram_tester_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;

 private:
  Persistent<NotificationImageLoader> loader_;
  NotificationLoadState loaded_ = NotificationLoadState::kNotLoaded;
};

TEST_F(NotificationImageLoaderTest, SuccessTest) {
  KURL url = RegisterMockedURL(kNotificationImageLoaderIcon500x500);
  LoadImage(url);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFinishTime.Icon", 0);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFileSize.Icon", 0);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFailTime.Icon", 0);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_EQ(NotificationLoadState::kLoadSuccessful, Loaded());
  histogram_tester_.ExpectTotalCount("Notifications.LoadFinishTime.Icon", 1);
  histogram_tester_.ExpectUniqueSample("Notifications.LoadFileSize.Icon", 7439,
                                       1);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFailTime.Icon", 0);
}

TEST_F(NotificationImageLoaderTest, TimeoutTest) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  // To test for a timeout, this needs to override the clock in the platform.
  // Just creating the mock platform will do everything to set it up.
  KURL url = RegisterMockedURL(kNotificationImageLoaderIcon500x500);
  LoadImage(url);

  // Run the platform for kImageFetchTimeoutInMs-1 seconds. This should not
  // result in a timeout.
  platform->RunForPeriodSeconds(kImageFetchTimeoutInMs / 1000 - 1);
  EXPECT_EQ(NotificationLoadState::kNotLoaded, Loaded());
  histogram_tester_.ExpectTotalCount("Notifications.LoadFinishTime.Icon", 0);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFileSize.Icon", 0);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFailTime.Icon", 0);

  // Now advance time until a timeout should be expected.
  platform->RunForPeriodSeconds(2);

  // If the loader times out, it calls the callback and returns an empty bitmap.
  EXPECT_EQ(NotificationLoadState::kLoadFailed, Loaded());
  histogram_tester_.ExpectTotalCount("Notifications.LoadFinishTime.Icon", 0);
  histogram_tester_.ExpectTotalCount("Notifications.LoadFileSize.Icon", 0);
  // Should log a non-zero failure time.
  histogram_tester_.ExpectTotalCount("Notifications.LoadFailTime.Icon", 1);
  histogram_tester_.ExpectBucketCount("Notifications.LoadFailTime.Icon", 0, 0);
}

}  // namspace
}  // namespace blink
