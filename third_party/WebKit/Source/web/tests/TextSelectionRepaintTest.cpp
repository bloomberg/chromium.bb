// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/editing/DOMSelection.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TextSelectionRepaintTest : public SimTest {};

TEST_F(TextSelectionRepaintTest, RepaintSelectionOnFocus) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(
      "<!DOCTYPE html>"
      "Text to select.");

  // Focus the window.
  EXPECT_FALSE(Page().IsFocused());
  Page().SetFocused(true);

  // First frame with nothing selected.
  Compositor().BeginFrame();

  // Select some text.
  auto* body = GetDocument().body();
  Window().getSelection()->setBaseAndExtent(body, 0, body, 1);

  // Unfocus the page and check for a pending frame.
  Page().SetFocused(false);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  // Frame with the unfocused selection appearance.
  Compositor().BeginFrame();

  // Focus the page and check for a pending frame.
  Page().SetFocused(true);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

}  // namespace blink
