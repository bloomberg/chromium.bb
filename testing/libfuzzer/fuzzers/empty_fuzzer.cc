// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Empty fuzzer that doesn't do anything. Used as test and documentation.

#include <stddef.h>

// Fuzzer entry point.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  // Run your code on data.
  return 0;
}

// Environment is optional.
struct Environment {
  Environment() {
    // Initialize your environment.
  }
};

Environment* env = new Environment();


