// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/stringprintf.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(HexEncode, ProducesEmptyStringFromEmptyByteArray) {
  const uint8_t kSomeMemoryLocation = 0;
  EXPECT_EQ("", HexEncode(absl::Span<const uint8_t>(&kSomeMemoryLocation, 0)));
}

TEST(HexEncode, ProducesHexStringsFromBytes) {
  const uint8_t kMessage[] = "Hello world!";
  const char kMessageInHex[] = "48656c6c6f20776f726c642100";
  EXPECT_EQ(kMessageInHex, HexEncode(kMessage));
}

}  // namespace
}  // namespace openscreen
