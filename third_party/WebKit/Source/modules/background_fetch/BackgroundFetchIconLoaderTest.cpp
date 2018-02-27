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

enum class LoadState { kNotLoaded, kLoadFailed, kLoadSuccessful };

constexpr char kImageLoaderBaseUrl[] = "http://test.com/";
constexpr char kImageLoaderBaseDir[] = "notifications/";
constexpr char kImageLoaderIcon500x500[] = "500x500.png";

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
        kImageLoaderBaseUrl, testing::CoreTestDataPath(kImageLoaderBaseDir),
        file_name, "image/png");
    return registered_url;
  }

  // Callback for BackgroundFetchIconLoader. This will set up the state of the
  // load as either success or failed based on whether the bitmap is empty.
  void IconLoaded(const SkBitmap& bitmap) {
    LOG(ERROR) << "did icon get loaded?";
    if (!bitmap.empty())
      loaded_ = LoadState::kLoadSuccessful;
    else
      loaded_ = LoadState::kLoadFailed;
  }

  void LoadIcon(const KURL& url) {
    loader_->Start(GetContext(), url,
                   Bind(&BackgroundFetchIconLoaderTest::IconLoaded,
                        WTF::Unretained(this)));
  }

  ExecutionContext* GetContext() const { return &GetDocument(); }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  LoadState loaded_ = LoadState::kNotLoaded;

 private:
  Persistent<BackgroundFetchIconLoader> loader_;
};

TEST_F(BackgroundFetchIconLoaderTest, SuccessTest) {
  KURL url = RegisterMockedURL(kImageLoaderIcon500x500);
  LoadIcon(url);
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_EQ(LoadState::kLoadSuccessful, loaded_);
}

}  // namespace
}  // namespace blink
