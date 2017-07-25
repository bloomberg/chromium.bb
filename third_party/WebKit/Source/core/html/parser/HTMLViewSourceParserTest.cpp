// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLViewSourceParser.h"

#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentParser.h"
#include "core/html/HTMLViewSourceDocument.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// This is a regression test for https://crbug.com/664915
TEST(HTMLViewSourceParserTest, DetachThenFinish_ShouldNotCrash) {
  String mime_type("text/html");
  HTMLViewSourceDocument* document =
      HTMLViewSourceDocument::Create(DocumentInit::Create(), mime_type);
  HTMLViewSourceParser* parser =
      HTMLViewSourceParser::Create(*document, mime_type);
  // A client may detach the parser from the document.
  parser->Detach();
  // A DocumentWriter may call finish() after detach().
  static_cast<DocumentParser*>(parser)->Finish();
  // The test passed if finish did not crash.
}

}  // namespace blink
