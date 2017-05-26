// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkFuzzerTestSupport_h
#define BlinkFuzzerTestSupport_h

namespace blink {

// Instantiating BlinkFuzzerTestSupport will spin up an environment similar to
// webkit_unit_tests. It should be statically initialized and leaked in fuzzers.
class BlinkFuzzerTestSupport {
 public:
  // Use this constructor in LLVMFuzzerTestOneInput.
  BlinkFuzzerTestSupport();

  // Use this constructor in LLVMFuzzerInitialize only if argv is necessary.
  BlinkFuzzerTestSupport(int argc, char** argv);
  ~BlinkFuzzerTestSupport();
};

}  // namespace blink

#endif  // BlinkFuzzerTestSupport_h
