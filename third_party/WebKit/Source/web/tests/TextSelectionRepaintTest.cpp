// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/editing/DOMSelection.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

class TextSelectionRepaintTest : public SimTest {};

TEST_F(TextSelectionRepaintTest, RepaintSelectionOnFocus) {
  SimRequest mainResource("https://example.com/test.html", "text/html");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "Text to select.");

  // Focus the window.
  EXPECT_FALSE(page().isFocused());
  page().setFocused(true);

  // First frame with nothing selected.
  compositor().beginFrame();

  // Select some text.
  auto* body = document().body();
  window().getSelection()->setBaseAndExtent(body, 0, body, 1);

  // Unfocus the page and check for a pending frame.
  page().setFocused(false);
  EXPECT_TRUE(compositor().needsBeginFrame());

  // Frame with the unfocused selection appearance.
  compositor().beginFrame();

  // Focus the page and check for a pending frame.
  page().setFocused(true);
  EXPECT_TRUE(compositor().needsBeginFrame());
}

}  // namespace blink
