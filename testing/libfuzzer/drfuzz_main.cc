// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

// Provide main for running fuzzer tests with Dr. Fuzz.
int main(int argc, char **argv)
{
  static const size_t kFuzzInputMaxSize = 1024;
  unsigned char* fuzz_input = new unsigned char[kFuzzInputMaxSize]();
  // The buffer and size arguments can be changed by Dr. Fuzz.
  int result = LLVMFuzzerTestOneInput(fuzz_input, kFuzzInputMaxSize);
  delete[] fuzz_input;
  return result;
}
