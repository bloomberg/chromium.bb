// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/canvas/HTMLCanvasElement.h"
#include "core/loader/EmptyClients.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "core/testing/PageTestBase.h"
#include "modules/canvas/htmlcanvas/HTMLCanvasElementModule.h"
#include "modules/canvas/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/mojom/page/page_visibility_state.mojom-blink.h"

using testing::Mock;

namespace blink {

class OffscreenCanvasTest : public PageTestBase {
 protected:
  OffscreenCanvasTest();
  void SetUp() override;
  void TearDown() override;

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  OffscreenCanvas& OSCanvas() const { return *offscreen_canvas_; }
  OffscreenCanvasFrameDispatcher* Dispatcher() const {
    return offscreen_canvas_->GetOrCreateFrameDispatcher();
  }
  OffscreenCanvasRenderingContext2D& Context() const { return *context_; }
  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }
  ScopedTestingPlatformSupport<TestingPlatformSupport>& platform() {
    return platform_;
  }

 private:
  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<OffscreenCanvas> offscreen_canvas_;
  Persistent<OffscreenCanvasRenderingContext2D> context_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  FakeGLES2Interface gl_;
};

OffscreenCanvasTest::OffscreenCanvasTest() = default;

void OffscreenCanvasTest::SetUp() {
  auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    *gpu_compositing_disabled = false;
    gl->SetIsContextLost(false);
    return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
  };
  SharedGpuContext::SetContextProviderFactoryForTesting(
      WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  PageTestBase::SetupPageWithClients(&page_clients);
  SetHtmlInnerHTML("<body><canvas id='c'></canvas></body>");
  canvas_element_ = ToHTMLCanvasElement(GetElementById("c"));
  DummyExceptionStateForTesting exception_state;
  offscreen_canvas_ = HTMLCanvasElementModule::transferControlToOffscreen(
      *canvas_element_, exception_state);
  CanvasContextCreationAttributesCore attrs;
  context_ = static_cast<OffscreenCanvasRenderingContext2D*>(
      offscreen_canvas_->GetCanvasRenderingContext(&GetDocument(), String("2d"),
                                                   attrs));
}

void OffscreenCanvasTest::TearDown() {
  SharedGpuContext::ResetForTesting();
}

TEST_F(OffscreenCanvasTest, AnimationNotInitiallySuspended) {
  ScriptState::Scope scope(GetScriptState());
  EXPECT_FALSE(Dispatcher()->IsAnimationSuspended());
}

TEST_F(OffscreenCanvasTest, AnimationActiveAfterCommit) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;
  EXPECT_FALSE(Dispatcher()->NeedsBeginFrame());
  Context().commit(GetScriptState(), exception_state);
  platform()->RunUntilIdle();
  EXPECT_TRUE(Dispatcher()->NeedsBeginFrame());
}

TEST_F(OffscreenCanvasTest, AnimationSuspendedWhilePlaceholderHidden) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  // Do a commit and run it to completion so that the frame dispatcher
  // instance becomes known to OffscreenCanvas (async).
  Context().commit(GetScriptState(), exception_state);  // necessary
  platform()->RunUntilIdle();
  EXPECT_FALSE(Dispatcher()->IsAnimationSuspended());

  // Change visibility to hidden -> animation should be suspended
  GetPage().SetVisibilityState(mojom::PageVisibilityState::kHidden, false);
  platform()->RunUntilIdle();
  EXPECT_TRUE(Dispatcher()->IsAnimationSuspended());

  // Change visibility to visible -> animation should resume
  GetPage().SetVisibilityState(mojom::PageVisibilityState::kVisible, false);
  platform()->RunUntilIdle();
  EXPECT_FALSE(Dispatcher()->IsAnimationSuspended());
}

}  // namespace blink
