// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/htmlcanvas/HTMLCanvasElementModule.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/loader/EmptyClients.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLCanvasElementModuleTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Page::PageClients page_clients;
    FillWithEmptyClients(page_clients);
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(800, 600), &page_clients);
    Persistent<Document> document = &dummy_page_holder->GetDocument();
    document->documentElement()->SetInnerHTMLFromString(
        "<body><canvas id='c'></canvas></body>");
    document->View()->UpdateAllLifecyclePhases();
    canvas_element_ = ToHTMLCanvasElement(document->getElementById("c"));
  }

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  OffscreenCanvas* TransferControlToOffscreen(ExceptionState&);

 private:
  Persistent<HTMLCanvasElement> canvas_element_;
};

OffscreenCanvas* HTMLCanvasElementModuleTest::TransferControlToOffscreen(
    ExceptionState& exception_state) {
  // This unit test only tests if the Canvas Id is associated correctly, so we
  // exclude the part that creates surface layer bridge because a mojo message
  // pipe cannot be tested using webkit unit tests.
  return HTMLCanvasElementModule::TransferControlToOffscreenInternal(
      CanvasElement(), exception_state);
}

TEST_F(HTMLCanvasElementModuleTest, TransferControlToOffscreen) {
  NonThrowableExceptionState exception_state;
  OffscreenCanvas* offscreen_canvas =
      TransferControlToOffscreen(exception_state);
  DOMNodeId canvas_id = offscreen_canvas->PlaceholderCanvasId();
  EXPECT_EQ(canvas_id, DOMNodeIds::IdForNode(&(CanvasElement())));
}

}  // namespace blink
