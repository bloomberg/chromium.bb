// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPainter.h"

#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/loader/EmptyClients.h"
#include "core/paint/StubChromeClientForSPv2.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/Canvas2DImageBufferSurface.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebSize.h"
#include "testing/gtest/include/gtest/gtest.h"

// Integration tests of canvas painting code (in SPv2 mode).

namespace blink {

class HTMLCanvasPainterTestForSPv2 : public ::testing::Test,
                                     public ::testing::WithParamInterface<bool>,
                                     private ScopedSlimmingPaintV2ForTest,
                                     private ScopedRootLayerScrollingForTest {
 public:
  HTMLCanvasPainterTestForSPv2()
      : ScopedSlimmingPaintV2ForTest(true),
        ScopedRootLayerScrollingForTest(GetParam()) {}

 protected:
  void SetUp() override {
    chrome_client_ = new StubChromeClientForSPv2();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    page_holder_ = DummyPageHolder::Create(
        IntSize(800, 600), &clients, nullptr, [](Settings& settings) {
          settings.SetAcceleratedCompositingEnabled(true);
          // LayoutHTMLCanvas doesn't exist if script is disabled.
          settings.SetScriptEnabled(true);
        });
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }
  bool HasLayerAttached(const WebLayer& layer) {
    return chrome_client_->HasLayer(layer);
  }

  PassRefPtr<Canvas2DLayerBridge> MakeCanvas2DLayerBridge(const IntSize& size) {
    return AdoptRef(new Canvas2DLayerBridge(
        WTF::WrapUnique(new FakeWebGraphicsContext3DProvider(&gl_)), size, 0,
        kNonOpaque, Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams()));
  }

 private:
  Persistent<StubChromeClientForSPv2> chrome_client_;
  FakeGLES2Interface gl_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

INSTANTIATE_TEST_CASE_P(All, HTMLCanvasPainterTestForSPv2, ::testing::Bool());

TEST_P(HTMLCanvasPainterTestForSPv2, Canvas2DLayerAppearsInLayerTree) {
  // Insert a <canvas> and force it into accelerated mode.
  GetDocument().body()->setInnerHTML("<canvas width=300 height=200>");
  HTMLCanvasElement* element =
      toHTMLCanvasElement(GetDocument().body()->firstChild());
  CanvasContextCreationAttributes attributes;
  attributes.setAlpha(true);
  CanvasRenderingContext* context =
      element->GetCanvasRenderingContext("2d", attributes);
  RefPtr<Canvas2DLayerBridge> bridge =
      MakeCanvas2DLayerBridge(IntSize(300, 200));
  element->CreateImageBufferUsingSurfaceForTesting(WTF::WrapUnique(
      new Canvas2DImageBufferSurface(bridge, IntSize(300, 200))));
  ASSERT_EQ(context, element->RenderingContext());
  ASSERT_TRUE(context->IsComposited());
  ASSERT_TRUE(element->IsAccelerated());

  // Force the page to paint.
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Fetch the layer associated with the <canvas>, and check that it was
  // correctly configured in the layer tree.
  const WebLayer* layer = context->PlatformLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  EXPECT_EQ(WebSize(300, 200), layer->Bounds());
}

}  // namespace blink
