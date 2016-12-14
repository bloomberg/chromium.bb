// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

namespace {

class ScrollbarsTest : private ScopedRootLayerScrollingForTest, public SimTest {
 public:
  ScrollbarsTest() : ScopedRootLayerScrollingForTest(true) {}
};

TEST_F(ScrollbarsTest, DocumentStyleRecalcPreservesScrollbars) {
  v8::HandleScope handleScope(v8::Isolate::GetCurrent());
  webView().resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");
  request.complete("<style> body { width: 1600px; height: 1200px; } </style>");
  PaintLayerScrollableArea* plsa = document().layoutView()->getScrollableArea();

  compositor().beginFrame();
  ASSERT_TRUE(plsa->verticalScrollbar() && plsa->horizontalScrollbar());

  // Forces recalc of LayoutView's computed style in Document::updateStyle,
  // without invalidating layout.
  mainFrame().executeScriptAndReturnValue(WebScriptSource(
      "document.querySelector('style').sheet.insertRule('body {}', 1);"));

  compositor().beginFrame();
  ASSERT_TRUE(plsa->verticalScrollbar() && plsa->horizontalScrollbar());
}

}  // namespace

}  // namespace blink
