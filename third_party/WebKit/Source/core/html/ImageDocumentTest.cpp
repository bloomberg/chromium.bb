// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/ImageDocument.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// An image of size 50x50.
Vector<unsigned char> JpegImage() {
  Vector<unsigned char> jpeg;

  static const unsigned char kData[] = {
      0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
      0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
      0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x32, 0x00, 0x32, 0x03,
      0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
      0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x15, 0x01, 0x01, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03,
      0x11, 0x00, 0x3f, 0x00, 0x00, 0x94, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xd9};

  jpeg.Append(kData, sizeof(kData));
  return jpeg;
}
}

class WindowToViewportScalingChromeClient : public EmptyChromeClient {
 public:
  WindowToViewportScalingChromeClient()
      : EmptyChromeClient(), scale_factor_(1.f) {}

  void SetScalingFactor(float s) { scale_factor_ = s; }
  float WindowToViewportScalar(const float s) const override {
    return s * scale_factor_;
  }

 private:
  float scale_factor_;
};

class ImageDocumentTest : public ::testing::Test {
 protected:
  void TearDown() override { ThreadState::Current()->CollectAllGarbage(); }

  void CreateDocumentWithoutLoadingImage(int view_width, int view_height);
  void CreateDocument(int view_width, int view_height);
  void LoadImage();

  ImageDocument& GetDocument() const;

  int ImageWidth() const { return GetDocument().ImageElement()->width(); }
  int ImageHeight() const { return GetDocument().ImageElement()->height(); }

  void SetPageZoom(float);
  void SetWindowToViewportScalingFactor(float);

 private:
  Persistent<WindowToViewportScalingChromeClient> chrome_client_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void ImageDocumentTest::CreateDocumentWithoutLoadingImage(int view_width,
                                                          int view_height) {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  chrome_client_ = new WindowToViewportScalingChromeClient();
  page_clients.chrome_client = chrome_client_;
  dummy_page_holder_ =
      DummyPageHolder::Create(IntSize(view_width, view_height), &page_clients);

  LocalFrame& frame = dummy_page_holder_->GetFrame();
  frame.GetDocument()->Shutdown();
  DocumentInit init = DocumentInit::Create().WithFrame(&frame);
  frame.DomWindow()->InstallNewDocument("image/jpeg", init, false);
}

void ImageDocumentTest::CreateDocument(int view_width, int view_height) {
  CreateDocumentWithoutLoadingImage(view_width, view_height);
  LoadImage();
}

ImageDocument& ImageDocumentTest::GetDocument() const {
  Document* document = dummy_page_holder_->GetFrame().DomWindow()->document();
  ImageDocument* image_document = static_cast<ImageDocument*>(document);
  return *image_document;
}

void ImageDocumentTest::LoadImage() {
  DocumentParser* parser = GetDocument().ImplicitOpen(
      ParserSynchronizationPolicy::kForceSynchronousParsing);
  const Vector<unsigned char>& data = JpegImage();
  parser->AppendBytes(reinterpret_cast<const char*>(data.data()), data.size());
  parser->Finish();
}

void ImageDocumentTest::SetPageZoom(float factor) {
  dummy_page_holder_->GetFrame().SetPageZoomFactor(factor);
}

void ImageDocumentTest::SetWindowToViewportScalingFactor(float factor) {
  chrome_client_->SetScalingFactor(factor);
}

TEST_F(ImageDocumentTest, ImageLoad) {
  CreateDocument(50, 50);
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, LargeImageScalesDown) {
  CreateDocument(25, 30);
  EXPECT_EQ(25, ImageWidth());
  EXPECT_EQ(25, ImageHeight());

  CreateDocument(35, 20);
  EXPECT_EQ(20, ImageWidth());
  EXPECT_EQ(20, ImageHeight());
}

TEST_F(ImageDocumentTest, RestoreImageOnClick) {
  CreateDocument(30, 40);
  GetDocument().ImageClicked(4, 4);
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, InitialZoomDoesNotAffectScreenFit) {
  CreateDocumentWithoutLoadingImage(20, 10);
  SetPageZoom(2.f);
  LoadImage();
  EXPECT_EQ(10, ImageWidth());
  EXPECT_EQ(10, ImageHeight());
  GetDocument().ImageClicked(4, 4);
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, ZoomingDoesNotChangeRelativeSize) {
  CreateDocument(75, 75);
  SetPageZoom(0.5f);
  GetDocument().WindowSizeChanged();
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
  SetPageZoom(2.f);
  GetDocument().WindowSizeChanged();
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, ImageScalesDownWithDsf) {
  CreateDocumentWithoutLoadingImage(20, 30);
  SetWindowToViewportScalingFactor(2.f);
  LoadImage();
  EXPECT_EQ(10, ImageWidth());
  EXPECT_EQ(10, ImageHeight());
}

TEST_F(ImageDocumentTest, ImageNotCenteredWithForceZeroLayoutHeight) {
  CreateDocumentWithoutLoadingImage(80, 70);
  GetDocument().GetPage()->GetSettings().SetForceZeroLayoutHeight(true);
  LoadImage();
  EXPECT_FALSE(GetDocument().ShouldShrinkToFit());
  EXPECT_EQ(0, GetDocument().ImageElement()->OffsetLeft());
  EXPECT_EQ(0, GetDocument().ImageElement()->OffsetTop());
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

TEST_F(ImageDocumentTest, ImageCenteredWithoutForceZeroLayoutHeight) {
  CreateDocumentWithoutLoadingImage(80, 70);
  GetDocument().GetPage()->GetSettings().SetForceZeroLayoutHeight(false);
  LoadImage();
  EXPECT_TRUE(GetDocument().ShouldShrinkToFit());
  EXPECT_EQ(15, GetDocument().ImageElement()->OffsetLeft());
  EXPECT_EQ(10, GetDocument().ImageElement()->OffsetTop());
  EXPECT_EQ(50, ImageWidth());
  EXPECT_EQ(50, ImageHeight());
}

}  // namespace blink
