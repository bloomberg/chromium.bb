// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLViewSourceParser.h"

#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentParser.h"
#include "core/html/HTMLViewSourceDocument.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

// This is a regression test for https://crbug.com/664915
TEST(HTMLViewSourceParserTest, DetachThenFinish_ShouldNotCrash) {
  String mimeType("text/html");
  HTMLViewSourceDocument* document =
      HTMLViewSourceDocument::create(DocumentInit(), mimeType);
  HTMLViewSourceParser* parser =
      HTMLViewSourceParser::create(*document, mimeType);
  // A client may detach the parser from the document.
  parser->detach();
  // A DocumentWriter may call finish() after detach().
  static_cast<DocumentParser*>(parser)->finish();
  // The test passed if finish did not crash.
}

}  // namespace blink
