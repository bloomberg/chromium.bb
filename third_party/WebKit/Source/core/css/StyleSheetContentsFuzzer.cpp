// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/StyleSheetContents.h"

#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/wtf/text/WTFString.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  blink::CSSParserContext* context =
      blink::CSSParserContext::Create(blink::kHTMLStandardMode);
  blink::StyleSheetContents* styleSheet =
      blink::StyleSheetContents::Create(context);

  styleSheet->ParseString(String::FromUTF8WithLatin1Fallback(
      reinterpret_cast<const char*>(data), size));

#if defined(ADDRESS_SANITIZER)
  // LSAN needs unreachable objects to be released to avoid reporting them
  // incorrectly as a memory leak.
  blink::ThreadState* currentThreadState = blink::ThreadState::Current();
  currentThreadState->CollectAllGarbage();
#endif

  return 0;
}

