// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

void MaybeAllowImagePlaceholder(DummyPageHolder* page_holder,
                                FetchParameters& params) {
  if (page_holder->GetFrame().IsClientLoFiAllowed(params.GetResourceRequest()))
    params.SetClientLoFiPlaceholder();
}

void DisableDataSaverHoldbackInSettings(Settings& settings) {
  settings.SetDataSaverHoldbackWebApi(false);
}

void EnableDataSaverHoldbackInSettings(Settings& settings) {
  settings.SetDataSaverHoldbackWebApi(true);
}

}  // namespace

class TestLocalFrameClient : public EmptyLocalFrameClient {
 public:
  explicit TestLocalFrameClient(
      WebURLRequest::PreviewsState frame_previews_state)
      : frame_previews_state_(frame_previews_state) {}
  ~TestLocalFrameClient() override = default;

  WebURLRequest::PreviewsState GetPreviewsStateForFrame() const override {
    return frame_previews_state_;
  }

 private:
  const WebURLRequest::PreviewsState frame_previews_state_;
};

class LocalFrameTest : public testing::Test {
 public:
  void TearDown() override {
    // Reset the global data saver setting to false at the end of the test.
    GetNetworkStateNotifier().SetSaveDataEnabled(false);
  }
};

TEST_F(LocalFrameTest, MaybeAllowPlaceholderImageUsesSpecifiedRequestValue) {
  ResourceRequest request1;
  request1.SetURL(KURL("http://insecure.com"));
  request1.SetPreviewsState(WebURLRequest::kClientLoFiOn);
  FetchParameters params1(request1);
  auto page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kPreviewsOff));
  MaybeAllowImagePlaceholder(page_holder.get(), params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetImageRequestOptimization());

  ResourceRequest request2;
  request2.SetURL(KURL("https://secure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsOff);
  FetchParameters params2(request2);
  auto page_holder2 = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kClientLoFiOn));
  MaybeAllowImagePlaceholder(page_holder2.get(), params2);
  EXPECT_EQ(FetchParameters::kNone, params2.GetImageRequestOptimization());
}

TEST_F(LocalFrameTest, MaybeAllowPlaceholderImageUsesFramePreviewsState) {
  ResourceRequest request1;
  request1.SetURL(KURL("http://insecure.com"));
  request1.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params1(request1);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kClientLoFiOn));
  MaybeAllowImagePlaceholder(page_holder.get(), params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetImageRequestOptimization());
  EXPECT_TRUE(page_holder->GetFrame().IsUsingDataSavingPreview());

  ResourceRequest request2;
  request2.SetURL(KURL("http://insecure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params2(request2);
  std::unique_ptr<DummyPageHolder> page_holder2 =
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kServerLitePageOn));
  MaybeAllowImagePlaceholder(page_holder2.get(), params2);
  EXPECT_EQ(FetchParameters::kNone, params2.GetImageRequestOptimization());
  EXPECT_FALSE(page_holder2->GetFrame().IsUsingDataSavingPreview());
}

TEST_F(LocalFrameTest,
       MaybeAllowPlaceholderImageConditionalOnSchemeForServerLoFi) {
  ResourceRequest request1;
  request1.SetURL(KURL("https://secure.com"));
  request1.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params1(request1);
  auto page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kServerLoFiOn |
                                                 WebURLRequest::kClientLoFiOn));
  MaybeAllowImagePlaceholder(page_holder.get(), params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetImageRequestOptimization());

  ResourceRequest request2;
  request2.SetURL(KURL("http://insecure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params2(request2);
  auto page_holder2 = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kServerLoFiOn |
                                                 WebURLRequest::kClientLoFiOn));
  MaybeAllowImagePlaceholder(page_holder2.get(), params2);
  EXPECT_EQ(FetchParameters::kNone, params2.GetImageRequestOptimization());
}

TEST_F(LocalFrameTest, IsUsingDataSavingPreview) {
  EXPECT_TRUE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kClientLoFiOn))
          ->GetFrame()
          .IsUsingDataSavingPreview());
  EXPECT_TRUE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kServerLoFiOn))
          ->GetFrame()
          .IsUsingDataSavingPreview());
  EXPECT_TRUE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kNoScriptOn))
          ->GetFrame()
          .IsUsingDataSavingPreview());

  EXPECT_FALSE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kPreviewsUnspecified))
          ->GetFrame()
          .IsUsingDataSavingPreview());
  EXPECT_FALSE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kPreviewsOff))
          ->GetFrame()
          .IsUsingDataSavingPreview());
  EXPECT_FALSE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kPreviewsNoTransform))
          ->GetFrame()
          .IsUsingDataSavingPreview());
  EXPECT_FALSE(
      DummyPageHolder::Create(IntSize(800, 600), nullptr,
                              MakeGarbageCollected<TestLocalFrameClient>(
                                  WebURLRequest::kServerLitePageOn))
          ->GetFrame()
          .IsUsingDataSavingPreview());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithFeatureDisabled) {
  ScopedLazyFrameLoadingForTest scoped_lazy_frame_loading_for_test(false);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr, nullptr, &DisableDataSaverHoldbackInSettings);
  EXPECT_FALSE(page_holder->GetFrame().IsLazyLoadingImageAllowed());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWhenNotRestricted) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedRestrictLazyImageLoadingToDataSaverForTest
      scoped_restrict_lazy_image_loading_to_data_saver_for_test_(false);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr, nullptr, &DisableDataSaverHoldbackInSettings);
  EXPECT_TRUE(page_holder->GetFrame().IsLazyLoadingImageAllowed());
}

TEST_F(LocalFrameTest,
       IsLazyLoadingImageAllowedWhenRestrictedWithDataSaverDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedRestrictLazyImageLoadingToDataSaverForTest
      scoped_restrict_lazy_image_loading_to_data_saver_for_test_(true);
  GetNetworkStateNotifier().SetSaveDataEnabled(false);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr, nullptr, &DisableDataSaverHoldbackInSettings);
  EXPECT_FALSE(page_holder->GetFrame().IsLazyLoadingImageAllowed());
}

TEST_F(LocalFrameTest,
       IsLazyLoadingImageAllowedWhenRestrictedWithDataSaverEnabledHoldback) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedRestrictLazyImageLoadingToDataSaverForTest
      scoped_restrict_lazy_image_loading_to_data_saver_for_test_(true);
  GetNetworkStateNotifier().SetSaveDataEnabled(true);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr, nullptr, &EnableDataSaverHoldbackInSettings);
  EXPECT_FALSE(page_holder->GetFrame().IsLazyLoadingImageAllowed());
}

TEST_F(LocalFrameTest,
       IsLazyLoadingImageAllowedWhenRestrictedWithDataSaverEnabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedRestrictLazyImageLoadingToDataSaverForTest
      scoped_restrict_lazy_image_loading_to_data_saver_for_test_(true);
  GetNetworkStateNotifier().SetSaveDataEnabled(true);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr, nullptr, &DisableDataSaverHoldbackInSettings);
  EXPECT_TRUE(page_holder->GetFrame().IsLazyLoadingImageAllowed());
}

}  // namespace blink
