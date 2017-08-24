// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/loader/EmptyClients.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/canvas/HTMLCanvasElementModule.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace blink {

class OffscreenCanvasTest : public ::testing::Test {
 protected:
  OffscreenCanvasTest();
  void SetUp() override;
  void TearDown() override;

  DummyPageHolder& Page() const { return *dummy_page_holder_; }
  Document& GetDocument() const { return *document_; }
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
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Document> document_;
  Persistent<HTMLCanvasElement> canvas_element_;
  Persistent<OffscreenCanvas> offscreen_canvas_;
  Persistent<OffscreenCanvasRenderingContext2D> context_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  FakeGLES2Interface gl_;
};

OffscreenCanvasTest::OffscreenCanvasTest() {}

void OffscreenCanvasTest::SetUp() {
  SharedGpuContext::SetContextProviderFactoryForTesting([this] {
    gl_.SetIsContextLost(false);
    return std::unique_ptr<WebGraphicsContext3DProvider>(
        new FakeWebGraphicsContext3DProvider(&gl_));
  });
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  dummy_page_holder_ =
      DummyPageHolder::Create(IntSize(800, 600), &page_clients);
  document_ = &dummy_page_holder_->GetDocument();
  document_->documentElement()->setInnerHTML(
      "<body><canvas id='c'></canvas></body>");
  document_->View()->UpdateAllLifecyclePhases();
  canvas_element_ = toHTMLCanvasElement(document_->getElementById("c"));
  DummyExceptionStateForTesting exception_state;
  offscreen_canvas_ = HTMLCanvasElementModule::transferControlToOffscreen(
      *canvas_element_, exception_state);
  CanvasContextCreationAttributes attrs;
  context_ = static_cast<OffscreenCanvasRenderingContext2D*>(
      offscreen_canvas_->GetCanvasRenderingContext(document_, String("2d"),
                                                   attrs));
}

void OffscreenCanvasTest::TearDown() {
  SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
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
  Page().GetPage().SetVisibilityState(kPageVisibilityStateHidden, false);
  platform()->RunUntilIdle();
  EXPECT_TRUE(Dispatcher()->IsAnimationSuspended());

  // Change visibility to visible -> animation should resume
  Page().GetPage().SetVisibilityState(kPageVisibilityStateVisible, false);
  platform()->RunUntilIdle();
  EXPECT_FALSE(Dispatcher()->IsAnimationSuspended());
}

}  // blink
