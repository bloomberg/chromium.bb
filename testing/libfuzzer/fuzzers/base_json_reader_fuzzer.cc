// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data,
                                      unsigned long size) {
  base::JSONReader reader;
  reader.Read(std::string(reinterpret_cast<const char*>(data), size));
  return 0;
}

