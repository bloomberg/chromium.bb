// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_overlay.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/skia/include/core/SkCanvas.h"

using testing::_;
using testing::AtLeast;
using testing::Property;

namespace blink {
namespace {

// FrameOverlay that paints a solid color.
class SolidColorOverlay : public FrameOverlay::Delegate {
 public:
  SolidColorOverlay(Color color) : color_(color) {}

  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const IntSize& size) const override {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, frame_overlay, DisplayItem::kFrameOverlay))
      return;
    FloatRect rect(0, 0, size.Width(), size.Height());
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay);
    graphics_context.FillRect(rect, color_);
  }

 private:
  Color color_;
};

class FrameOverlayTest : public testing::Test, public PaintTestConfigurations {
 protected:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  FrameOverlayTest() {
    helper_.Initialize(nullptr, nullptr, nullptr);
    GetWebView()->MainFrameWidget()->Resize(
        WebSize(kViewportWidth, kViewportHeight));
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }

  std::unique_ptr<FrameOverlay> CreateSolidYellowOverlay() {
    return std::make_unique<FrameOverlay>(
        GetWebView()->MainFrameImpl()->GetFrame(),
        std::make_unique<SolidColorOverlay>(SK_ColorYELLOW));
  }

  template <typename OverlayType>
  void RunFrameOverlayTestWithAcceleratedCompositing();

 private:
  frame_test_helpers::WebViewHelper helper_;
};

class MockFrameOverlayCanvas : public SkCanvas {
 public:
  MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

INSTANTIATE_PAINT_TEST_SUITE_P(FrameOverlayTest);

TEST_P(FrameOverlayTest, AcceleratedCompositing) {
  std::unique_ptr<FrameOverlay> frame_overlay = CreateSolidYellowOverlay();
  frame_overlay->Update();
  GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  // Ideally, we would get results from the compositor that showed that this
  // page overlay actually winds up getting drawn on top of the rest.
  // For now, we just check that the GraphicsLayer will draw the right thing.
  MockFrameOverlayCanvas canvas;
  EXPECT_CALL(canvas,
              onDrawRect(SkRect::MakeWH(kViewportWidth, kViewportHeight),
                         Property(&SkPaint::getColor, SK_ColorYELLOW)));

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintRecordBuilder builder;
    frame_overlay->Paint(builder.Context());
    builder.EndRecording()->Playback(&canvas);
  } else {
    auto* graphics_layer = frame_overlay->GetGraphicsLayer();
    graphics_layer->Paint();
    graphics_layer->CapturePaintRecord()->Playback(&canvas);
  }
}

TEST_P(FrameOverlayTest, VisualRect) {
  std::unique_ptr<FrameOverlay> frame_overlay = CreateSolidYellowOverlay();
  frame_overlay->Update();
  GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);
  EXPECT_EQ(LayoutRect(0, 0, kViewportWidth, kViewportHeight),
            frame_overlay->VisualRect());
}

}  // namespace
}  // namespace blink
