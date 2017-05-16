// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/html/HTMLLinkElement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

class HTMLImportSheetsTest : public SimTest {
 protected:
  HTMLImportSheetsTest() { WebView().Resize(WebSize(640, 480)); }
};

TEST_F(HTMLImportSheetsTest, NeedsActiveStyleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest import_resource("https://example.com/import.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<link id=link rel=import href=import.html>");
  import_resource.Complete("<style>div{}</style>");

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsActiveStyleUpdate());
}

}  // namespace blink
