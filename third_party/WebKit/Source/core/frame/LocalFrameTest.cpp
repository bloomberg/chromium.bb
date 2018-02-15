// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrame.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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

TEST(LocalFrameTest, MaybeAllowPlaceholderImageUsesSpecifiedRequestValue) {
  ResourceRequest request1;
  request1.SetURL(KURL("http://insecure.com"));
  request1.SetPreviewsState(WebURLRequest::kClientLoFiOn);
  FetchParameters params1(request1);
  DummyPageHolder::Create(IntSize(800, 600), nullptr,
                          new TestLocalFrameClient(WebURLRequest::kPreviewsOff))
      ->GetFrame()
      .MaybeAllowImagePlaceholder(params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetPlaceholderImageRequestType());

  ResourceRequest request2;
  request2.SetURL(KURL("https://secure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsOff);
  FetchParameters params2(request2);
  DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      new TestLocalFrameClient(WebURLRequest::kClientLoFiOn))
      ->GetFrame()
      .MaybeAllowImagePlaceholder(params2);
  EXPECT_EQ(FetchParameters::kDisallowPlaceholder,
            params2.GetPlaceholderImageRequestType());
}

TEST(LocalFrameTest, MaybeAllowPlaceholderImageUsesFramePreviewsState) {
  ResourceRequest request1;
  request1.SetURL(KURL("http://insecure.com"));
  request1.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params1(request1);
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      new TestLocalFrameClient(WebURLRequest::kClientLoFiOn));
  page_holder->GetFrame().MaybeAllowImagePlaceholder(params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetPlaceholderImageRequestType());
  EXPECT_TRUE(page_holder->GetFrame().IsUsingDataSavingPreview());

  ResourceRequest request2;
  request2.SetURL(KURL("http://insecure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params2(request2);
  std::unique_ptr<DummyPageHolder> page_holder2 = DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      new TestLocalFrameClient(WebURLRequest::kServerLitePageOn));
  page_holder2->GetFrame().MaybeAllowImagePlaceholder(params2);
  EXPECT_EQ(FetchParameters::kDisallowPlaceholder,
            params2.GetPlaceholderImageRequestType());
  EXPECT_FALSE(page_holder2->GetFrame().IsUsingDataSavingPreview());
}

TEST(LocalFrameTest,
     MaybeAllowPlaceholderImageConditionalOnSchemeForServerLoFi) {
  ResourceRequest request1;
  request1.SetURL(KURL("https://secure.com"));
  request1.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params1(request1);
  DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      new TestLocalFrameClient(WebURLRequest::kServerLoFiOn |
                               WebURLRequest::kClientLoFiOn))
      ->GetFrame()
      .MaybeAllowImagePlaceholder(params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetPlaceholderImageRequestType());

  ResourceRequest request2;
  request2.SetURL(KURL("http://insecure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params2(request2);
  DummyPageHolder::Create(
      IntSize(800, 600), nullptr,
      new TestLocalFrameClient(WebURLRequest::kServerLoFiOn |
                               WebURLRequest::kClientLoFiOn))
      ->GetFrame()
      .MaybeAllowImagePlaceholder(params2);
  EXPECT_EQ(FetchParameters::kDisallowPlaceholder,
            params2.GetPlaceholderImageRequestType());
}

TEST(LocalFrameTest, IsUsingDataSavingPreview) {
  EXPECT_TRUE(DummyPageHolder::Create(
                  IntSize(800, 600), nullptr,
                  new TestLocalFrameClient(WebURLRequest::kClientLoFiOn))
                  ->GetFrame()
                  .IsUsingDataSavingPreview());
  EXPECT_TRUE(DummyPageHolder::Create(
                  IntSize(800, 600), nullptr,
                  new TestLocalFrameClient(WebURLRequest::kServerLoFiOn))
                  ->GetFrame()
                  .IsUsingDataSavingPreview());
  EXPECT_TRUE(DummyPageHolder::Create(
                  IntSize(800, 600), nullptr,
                  new TestLocalFrameClient(WebURLRequest::kNoScriptOn))
                  ->GetFrame()
                  .IsUsingDataSavingPreview());

  EXPECT_FALSE(DummyPageHolder::Create(IntSize(800, 600), nullptr,
                                       new TestLocalFrameClient(
                                           WebURLRequest::kPreviewsUnspecified))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
  EXPECT_FALSE(DummyPageHolder::Create(
                   IntSize(800, 600), nullptr,
                   new TestLocalFrameClient(WebURLRequest::kPreviewsOff))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
  EXPECT_FALSE(DummyPageHolder::Create(IntSize(800, 600), nullptr,
                                       new TestLocalFrameClient(
                                           WebURLRequest::kPreviewsNoTransform))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
  EXPECT_FALSE(DummyPageHolder::Create(
                   IntSize(800, 600), nullptr,
                   new TestLocalFrameClient(WebURLRequest::kServerLitePageOn))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
}

}  // namespace blink
