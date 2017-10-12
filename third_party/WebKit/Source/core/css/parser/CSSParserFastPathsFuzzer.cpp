// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserFastPaths.h"

#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/wtf/text/WTFString.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();

  // MaybeParseValue assumes the string is not empty.
  if (size == 0)
    return 0;

  const std::string data_string(reinterpret_cast<const char*>(data), size);
  const std::size_t data_hash = std::hash<std::string>()(data_string);

  const auto property_id = blink::convertToCSSPropertyID(
      (data_hash % blink::numCSSProperties) + blink::firstCSSProperty);

  for (unsigned parser_mode = 0;
       parser_mode < blink::CSSParserMode::kNumCSSParserModes; parser_mode++) {
    blink::CSSParserFastPaths::MaybeParseValue(
        property_id, String::FromUTF8WithLatin1Fallback(data, size),
        static_cast<blink::CSSParserMode>(parser_mode));
  }

#if defined(ADDRESS_SANITIZER)
  // LSAN needs unreachable objects to be released to avoid reporting them
  // incorrectly as a memory leak.
  blink::ThreadState* currentThreadState = blink::ThreadState::Current();
  currentThreadState->CollectAllGarbage();
#endif

  return 0;
}
