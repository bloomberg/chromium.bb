// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/ImageDocument.h"

#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/geometry/DOMRect.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
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

#if defined(OS_ANDROID)
#define MAYBE(test) DISABLED_##test
#else
#define MAYBE(test) test
#endif

TEST_F(ImageDocumentTest, MAYBE(ImageCenteredAtDeviceScaleFactor)) {
  CreateDocumentWithoutLoadingImage(30, 30);
  SetWindowToViewportScalingFactor(1.5f);
  LoadImage();

  EXPECT_TRUE(GetDocument().ShouldShrinkToFit());
  GetDocument().ImageClicked(15, 27);
  ScrollOffset offset = GetDocument()
                            .GetFrame()
                            ->View()
                            ->LayoutViewportScrollableArea()
                            ->GetScrollOffset();
  EXPECT_EQ(22, offset.Width());
  EXPECT_EQ(42, offset.Height());

  GetDocument().ImageClicked(20, 20);

  GetDocument().ImageClicked(12, 15);
  offset = GetDocument()
               .GetFrame()
               ->View()
               ->LayoutViewportScrollableArea()
               ->GetScrollOffset();
  EXPECT_EQ(11, offset.Width());
  EXPECT_EQ(22, offset.Height());
}

class ImageDocumentViewportTest : public SimTest {
 public:
  ImageDocumentViewportTest() = default;
  ~ImageDocumentViewportTest() override = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetViewportMetaEnabled(true);
    WebView().GetSettings()->SetShrinksViewportContentToFit(true);
    WebView().GetSettings()->SetMainFrameResizesAreOrientationChanges(true);
  }

  VisualViewport& GetVisualViewport() {
    return WebView().GetPage()->GetVisualViewport();
  }

  ImageDocument& GetDocument() {
    Document* document =
        ToLocalFrame(WebView().GetPage()->MainFrame())->DomWindow()->document();
    ImageDocument* image_document = static_cast<ImageDocument*>(document);
    return *image_document;
  }
};

// Tests that hiding the URL bar doesn't cause a "jump" when viewing an image
// much wider than the viewport.
TEST_F(ImageDocumentViewportTest, HidingURLBarDoesntChangeImageLocation) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  // Initialize with the URL bar showing. Make the viewport very thin so that
  // we load an image much wider than the viewport but fits vertically. The
  // page will load zoomed out so the image will be vertically centered.
  WebView().ResizeWithBrowserControls(IntSize(5, 40), 10, 0, true);
  SimRequest request("https://example.com/test.jpg", "image/jpeg");
  LoadURL("https://example.com/test.jpg");

  Vector<unsigned char> jpeg = JpegImage();
  Vector<char> data = Vector<char>();
  data.Append(jpeg.data(), jpeg.size());
  request.Complete(data);

  Compositor().BeginFrame();

  HTMLImageElement* img = GetDocument().ImageElement();
  DOMRect* rect = img->getBoundingClientRect();

  // Some initial sanity checking. We'll use the BoundingClientRect for the
  // image location since that's relative to the layout viewport and the layout
  // viewport is unscrollable in this test. Since the image is 10X wider than
  // the viewport, we'll zoom out to 0.1. This means the layout viewport is 400
  // pixels high so the image will be centered in that.
  ASSERT_EQ(50u, img->width());
  ASSERT_EQ(50u, img->height());
  ASSERT_EQ(0.1f, GetVisualViewport().Scale());
  ASSERT_EQ(0, rect->x());
  ASSERT_EQ(175, rect->y());

  // Hide the URL bar. This will make the viewport taller but won't change the
  // layout size so the image location shouldn't change.
  WebView().ResizeWithBrowserControls(IntSize(5, 50), 10, 0, false);
  Compositor().BeginFrame();
  rect = img->getBoundingClientRect();
  EXPECT_EQ(0, rect->x());
  EXPECT_EQ(175, rect->y());
}

#undef MAYBE
}  // namespace blink
