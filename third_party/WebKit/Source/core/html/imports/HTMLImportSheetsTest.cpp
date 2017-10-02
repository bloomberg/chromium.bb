// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/html/HTMLLinkElement.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLImportSheetsTest : public SimTest {
 protected:
  HTMLImportSheetsTest() {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().Resize(WebSize(640, 480));
  }
};

TEST_F(HTMLImportSheetsTest, NeedsActiveStyleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest import_resource("https://example.com/import.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<style>div{}</style>");

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  Document* import_doc =
      ToHTMLLinkElement(GetDocument().getElementById("link"))->import();
  ASSERT_TRUE(import_doc);
  EXPECT_TRUE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());

  GetDocument().GetStyleEngine().UpdateActiveStyle();

  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  EXPECT_FALSE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(HTMLImportSheetsTest, UpdateStyleSheetList) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest import_resource("https://example.com/import.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<style>div{}</style>");

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  Document* import_doc =
      ToHTMLLinkElement(GetDocument().getElementById("link"))->import();
  ASSERT_TRUE(import_doc);
  EXPECT_TRUE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());

  import_doc->GetStyleEngine().StyleSheetsForStyleSheetList(*import_doc);

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
  EXPECT_TRUE(import_doc->GetStyleEngine().NeedsActiveStyleUpdate());
}

}  // namespace blink
