// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/html/HTMLLinkElement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

class LinkElementLoadingTest : public SimTest {};

TEST_F(LinkElementLoadingTest,
       ShouldCancelLoadingStyleSheetIfLinkElementIsDisconnected) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssResource("https://example.com/test.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.start();
  mainResource.write(
      "<!DOCTYPE html><link id=link rel=stylesheet href=test.css>");

  // Sheet is streaming in, but not ready yet.
  cssResource.start();

  // Remove a link element from a document
  HTMLLinkElement* link = toHTMLLinkElement(document().getElementById("link"));
  EXPECT_NE(nullptr, link);
  link->remove();

  // Finish the load.
  cssResource.complete();
  mainResource.finish();

  // Link element's sheet loading should be canceled.
  EXPECT_EQ(nullptr, link->sheet());
}

}  // namespace blink
