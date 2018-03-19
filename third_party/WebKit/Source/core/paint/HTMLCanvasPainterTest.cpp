// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPainter.h"

#include <memory>
#include <utility>

#include "core/frame/LocalFrameView.h"
#include "core/html/canvas/CanvasContextCreationAttributesCore.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/HTMLCanvasElement.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/StubChromeClientForSPv2.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/testing/runtime_enabled_features_test_helpers.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebSize.h"

#include "platform/scroll/ScrollbarTheme.h"

// Integration tests of canvas painting code (in SPv2 mode).

namespace blink {

class HTMLCanvasPainterTestForSPv2 : public PaintControllerPaintTest {
 public:
  HTMLCanvasPainterTestForSPv2()
      : chrome_client_(new StubChromeClientForSPv2) {}

 protected:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
    PaintControllerPaintTest::SetUp();
  }

  void TearDown() override {
    SharedGpuContext::ResetForTesting();
    PaintControllerPaintTest::TearDown();
  }

  FrameSettingOverrideFunction SettingOverrider() const override {
    return [](Settings& settings) {
      settings.SetAcceleratedCompositingEnabled(true);
      // LayoutHTMLCanvas doesn't exist if script is disabled.
      settings.SetScriptEnabled(true);
    };
  }

  ChromeClient& GetChromeClient() const override { return *chrome_client_; }

  bool HasLayerAttached(const WebLayer& layer) {
    return chrome_client_->HasLayer(layer);
  }

  std::unique_ptr<Canvas2DLayerBridge> MakeCanvas2DLayerBridge(
      const IntSize& size) {
    return std::make_unique<Canvas2DLayerBridge>(
        size, 0, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams());
  }

 private:
  Persistent<StubChromeClientForSPv2> chrome_client_;
  FakeGLES2Interface gl_;
};

INSTANTIATE_TEST_CASE_P(
    All,
    HTMLCanvasPainterTestForSPv2,
    ::testing::ValuesIn(kSlimmingPaintV2TestConfigurations));

TEST_P(HTMLCanvasPainterTestForSPv2, Canvas2DLayerAppearsInLayerTree) {
  // Insert a <canvas> and force it into accelerated mode.
  // Not using SetBodyInnerHTML() because we need to test before document
  // lifecyle update.
  GetDocument().body()->SetInnerHTMLFromString("<canvas width=300 height=200>");
  HTMLCanvasElement* element =
      ToHTMLCanvasElement(GetDocument().body()->firstChild());
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  CanvasRenderingContext* context =
      element->GetCanvasRenderingContext("2d", attributes);
  IntSize size(300, 200);
  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeCanvas2DLayerBridge(size);
  element->CreateCanvas2DLayerBridgeForTesting(std::move(bridge), size);
  ASSERT_EQ(context, element->RenderingContext());
  ASSERT_TRUE(context->IsComposited());
  ASSERT_TRUE(element->IsAccelerated());

  // Force the page to paint.
  element->FinalizeFrame();
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Fetch the layer associated with the <canvas>, and check that it was
  // correctly configured in the layer tree.
  const WebLayer* layer = context->PlatformLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  EXPECT_EQ(WebSize(300, 200), layer->Bounds());
}

}  // namespace blink
