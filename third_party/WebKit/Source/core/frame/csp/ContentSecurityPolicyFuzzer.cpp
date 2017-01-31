// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/ContentSecurityPolicy.h"

#include "core/testing/DummyPageHolder.h"
#include "platform/heap/ThreadState.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Intentionally leaked during fuzzing.
// See testing/libfuzzer/efficient_fuzzer.md.
DummyPageHolder* pageHolder = nullptr;

int LLVMFuzzerInitialize(int* argc, char*** argv) {
  InitializeBlinkFuzzTest(argc, argv);
  pageHolder = DummyPageHolder::create().release();
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  String header = String::fromUTF8(data, size);
  unsigned hash = header.isNull() ? 0 : header.impl()->hash();

  // Construct and initialize a policy from the string.
  ContentSecurityPolicy* csp = ContentSecurityPolicy::create();
  csp->didReceiveHeader(header,
                        hash & 0x01 ? ContentSecurityPolicyHeaderTypeEnforce
                                    : ContentSecurityPolicyHeaderTypeReport,
                        hash & 0x02 ? ContentSecurityPolicyHeaderSourceHTTP
                                    : ContentSecurityPolicyHeaderSourceMeta);
  pageHolder->document().initContentSecurityPolicy(csp);

  // Force a garbage collection.
  // Specify namespace explicitly. Otherwise it conflicts on Mac OS X with:
  // CoreServices.framework/Frameworks/CarbonCore.framework/Headers/Threads.h.
  blink::ThreadState::current()->collectGarbage(
      BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  return blink::LLVMFuzzerInitialize(argc, argv);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
