// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/PageOverlay.h"

#include <memory>
#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebThread.h"
#include "public/web/WebSettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Property;

namespace blink {
namespace {

static const int kViewportWidth = 800;
static const int kViewportHeight = 600;

// These unit tests cover both PageOverlay and PageOverlayList.

void EnableAcceleratedCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(true);
}

void DisableAcceleratedCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(false);
}

// PageOverlay that paints a solid color.
class SolidColorOverlay : public PageOverlay::Delegate {
 public:
  SolidColorOverlay(Color color) : color_(color) {}

  void PaintPageOverlay(const PageOverlay& page_overlay,
                        GraphicsContext& graphics_context,
                        const WebSize& size) const override {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, page_overlay, DisplayItem::kPageOverlay))
      return;
    FloatRect rect(0, 0, size.width, size.height);
    DrawingRecorder recorder(graphics_context, page_overlay,
                             DisplayItem::kPageOverlay);
    graphics_context.FillRect(rect, color_);
  }

 private:
  Color color_;
};

class PageOverlayTest : public ::testing::Test {
 protected:
  enum CompositingMode { kAcceleratedCompositing, kUnacceleratedCompositing };

  void Initialize(CompositingMode compositing_mode) {
    helper_.Initialize(nullptr /* webFrameClient */,
                       nullptr /* webViewClient */,
                       nullptr /* webWidgetClient */,
                       compositing_mode == kAcceleratedCompositing
                           ? EnableAcceleratedCompositing
                           : DisableAcceleratedCompositing);
    GetWebView()->Resize(WebSize(kViewportWidth, kViewportHeight));
    GetWebView()->UpdateAllLifecyclePhases();
    ASSERT_EQ(compositing_mode == kAcceleratedCompositing,
              GetWebView()->IsAcceleratedCompositingActive());
  }

  WebViewImpl* GetWebView() const { return helper_.WebView(); }

  std::unique_ptr<PageOverlay> CreateSolidYellowOverlay() {
    return PageOverlay::Create(
        GetWebView()->MainFrameImpl(),
        std::make_unique<SolidColorOverlay>(SK_ColorYELLOW));
  }

  void SetViewportSize(const WebSize& size) { helper_.SetViewportSize(size); }

  template <typename OverlayType>
  void RunPageOverlayTestWithAcceleratedCompositing();

 private:
  FrameTestHelpers::WebViewHelper helper_;
};

template <bool (*getter)(), void (*setter)(bool)>
class RuntimeFeatureChange {
 public:
  RuntimeFeatureChange(bool new_value) : old_value_(getter()) {
    setter(new_value);
  }
  ~RuntimeFeatureChange() { setter(old_value_); }

 private:
  bool old_value_;
};

class MockPageOverlayCanvas : public SkCanvas {
 public:
  MockPageOverlayCanvas(int width, int height) : SkCanvas(width, height) {}
  MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

TEST_F(PageOverlayTest, PageOverlay_AcceleratedCompositing) {
  Initialize(kAcceleratedCompositing);
  SetViewportSize(WebSize(kViewportWidth, kViewportHeight));

  std::unique_ptr<PageOverlay> page_overlay = CreateSolidYellowOverlay();
  page_overlay->Update();
  GetWebView()->UpdateAllLifecyclePhases();

  // Ideally, we would get results from the compositor that showed that this
  // page overlay actually winds up getting drawn on top of the rest.
  // For now, we just check that the GraphicsLayer will draw the right thing.

  MockPageOverlayCanvas canvas(kViewportWidth, kViewportHeight);
  EXPECT_CALL(canvas, onDrawRect(_, _)).Times(AtLeast(0));
  EXPECT_CALL(canvas,
              onDrawRect(SkRect::MakeWH(kViewportWidth, kViewportHeight),
                         Property(&SkPaint::getColor, SK_ColorYELLOW)));

  GraphicsLayer* graphics_layer = page_overlay->GetGraphicsLayer();
  WebRect rect(0, 0, kViewportWidth, kViewportHeight);

  // Paint the layer with a null canvas to get a display list, and then
  // replay that onto the mock canvas for examination.
  IntRect int_rect = rect;
  graphics_layer->Paint(&int_rect);
  canvas.drawPicture(
      ToSkPicture(graphics_layer->CapturePaintRecord(), int_rect));
}

TEST_F(PageOverlayTest, PageOverlay_VisualRect) {
  Initialize(kAcceleratedCompositing);
  std::unique_ptr<PageOverlay> page_overlay = CreateSolidYellowOverlay();
  page_overlay->Update();
  GetWebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 0, kViewportWidth, kViewportHeight),
            page_overlay->VisualRect());
}

}  // namespace
}  // namespace blink
