// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include "platform/heap/Handle.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support =
      blink::BlinkFuzzerTestSupport();
  WTF::Vector<WTF::String> messages;
  // TODO(csharrison): Be smarter about parsing this origin for performance.
  RefPtr<blink::SecurityOrigin> origin =
      blink::SecurityOrigin::CreateFromString("https://example.com/");
  blink::ParseFeaturePolicyHeader(WTF::String(data, size), origin.Get(),
                                  &messages);
  return 0;
}
