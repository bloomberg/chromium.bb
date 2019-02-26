// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/hpack/tools/hpack_example.h"

#include <ctype.h>

#include "base/logging.h"
#include "net/third_party/http2/platform/api/http2_bug_tracker.h"
#include "net/third_party/http2/platform/api/http2_string_utils.h"

namespace http2 {
namespace test {
namespace {

void HpackExampleToStringOrDie(Http2StringPiece example, Http2String* output) {
  while (!example.empty()) {
    const char c0 = example[0];
    if (isxdigit(c0)) {
      CHECK_GT(example.size(), 1u) << "Truncated hex byte?";
      const char c1 = example[1];
      CHECK(isxdigit(c1)) << "Found half a byte?";
      *output += Http2HexDecode(example.substr(0, 2));
      example.remove_prefix(2);
      continue;
    }
    if (isspace(c0)) {
      example.remove_prefix(1);
      continue;
    }
    if (!example.empty() && example[0] == '|') {
      // Start of a comment. Skip to end of line or of input.
      auto pos = example.find('\n');
      if (pos == Http2StringPiece::npos) {
        // End of input.
        break;
      }
      example.remove_prefix(pos + 1);
      continue;
    }
    HTTP2_BUG << "Can't parse byte " << static_cast<int>(c0)
              << Http2StrCat(" (0x", Http2Hex(c0), ")")
              << "\nExample: " << example;
  }
  CHECK_LT(0u, output->size()) << "Example is empty.";
}

}  // namespace

Http2String HpackExampleToStringOrDie(Http2StringPiece example) {
  Http2String output;
  HpackExampleToStringOrDie(example, &output);
  return output;
}

}  // namespace test
}  // namespace http2
