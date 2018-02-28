// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LiICENSE file.

#include "core/dom/ExecutionContext.h"
#include "core/testing/PageTestBase.h"
#include "modules/background_fetch/BackgroundFetchIconLoader.h"
#include "platform/heap/Persistent.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

enum class BackgroundFetchLoadState {
  kNotLoaded,
  kLoadFailed,
  kLoadSuccessful
};

constexpr char kBackgroundFetchImageLoaderBaseUrl[] = "http://test.com/";
constexpr char kBackgroundFetchImageLoaderBaseDir[] = "notifications/";
constexpr char kBackgroundFetchImageLoaderIcon500x500[] = "500x500.png";

class BackgroundFetchIconLoaderTest : public PageTestBase {
 public:
  BackgroundFetchIconLoaderTest() : loader_(new BackgroundFetchIconLoader()) {}
  ~BackgroundFetchIconLoaderTest() override {
    loader_->Stop();
    platform_->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetUp() override { PageTestBase::SetUp(IntSize()); }

  // Registers a mocked URL.
  WebURL RegisterMockedURL(const String& file_name) {
    WebURL registered_url = URLTestHelpers::RegisterMockedURLLoadFromBase(
        kBackgroundFetchImageLoaderBaseUrl,
        testing::CoreTestDataPath(kBackgroundFetchImageLoaderBaseDir),
        file_name, "image/png");
    return registered_url;
  }

  // Callback for BackgroundFetchIconLoader. This will set up the state of the
  // load as either success or failed based on whether the bitmap is empty.
  void IconLoaded(const SkBitmap& bitmap) {
    LOG(ERROR) << "did icon get loaded?";
    if (!bitmap.empty())
      loaded_ = BackgroundFetchLoadState::kLoadSuccessful;
    else
      loaded_ = BackgroundFetchLoadState::kLoadFailed;
  }

  void LoadIcon(const KURL& url) {
    loader_->Start(GetContext(), url,
                   Bind(&BackgroundFetchIconLoaderTest::IconLoaded,
                        WTF::Unretained(this)));
  }

  ExecutionContext* GetContext() const { return &GetDocument(); }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  BackgroundFetchLoadState loaded_ = BackgroundFetchLoadState::kNotLoaded;

 private:
  Persistent<BackgroundFetchIconLoader> loader_;
};

TEST_F(BackgroundFetchIconLoaderTest, SuccessTest) {
  KURL url = RegisterMockedURL(kBackgroundFetchImageLoaderIcon500x500);
  LoadIcon(url);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_EQ(BackgroundFetchLoadState::kLoadSuccessful, loaded_);
}

}  // namespace
}  // namespace blink
