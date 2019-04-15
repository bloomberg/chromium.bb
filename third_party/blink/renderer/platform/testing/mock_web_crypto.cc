// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/mock_web_crypto.h"

#include <cstring>
#include <memory>
#include <string>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// MemEq(p, len) expects memcmp(arg, p, len) == 0, where |arg| is the argument
// to be matched.
MATCHER_P2(MemEq,
           p,
           len,
           std::string("pointing to memory") + (negation ? " not" : "") +
               " equal to \"" + std::string(static_cast<const char*>(p), len) +
               "\" (length=" + testing::PrintToString(len) + ")") {
  return memcmp(arg, p, len) == 0;
}

}  // namespace blink
