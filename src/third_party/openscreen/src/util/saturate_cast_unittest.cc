// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/saturate_cast.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(SaturateCastTest, LargerToSmallerSignedInteger) {
  struct ValuePair {
    int64_t from;
    int32_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<int64_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<int64_t>::max() / 2 + 42,
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<int32_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {42, 42},
      {0, 0},
      {-42, -42},
      {std::numeric_limits<int32_t>::min(),
       std::numeric_limits<int32_t>::min()},
      {std::numeric_limits<int64_t>::min() / 2 - 42,
       std::numeric_limits<int32_t>::min()},
      {std::numeric_limits<int64_t>::min(),
       std::numeric_limits<int32_t>::min()},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<int32_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, LargerToSmallerUnsignedInteger) {
  struct ValuePair {
    uint64_t from;
    uint32_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<uint64_t>::max(),
       std::numeric_limits<uint32_t>::max()},
      {std::numeric_limits<uint64_t>::max() / 2 + 42,
       std::numeric_limits<uint32_t>::max()},
      {std::numeric_limits<uint32_t>::max(),
       std::numeric_limits<uint32_t>::max()},
      {42, 42},
      {0, 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<uint32_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, LargerSignedToSmallerUnsignedInteger) {
  struct ValuePair {
    int64_t from;
    uint32_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<int64_t>::max(),
       std::numeric_limits<uint32_t>::max()},
      {std::numeric_limits<int64_t>::max() / 2 + 42,
       std::numeric_limits<uint32_t>::max()},
      {std::numeric_limits<uint32_t>::max(),
       std::numeric_limits<uint32_t>::max()},
      {42, 42},
      {0, 0},
      {-42, 0},
      {std::numeric_limits<int64_t>::min() / 2 - 42, 0},
      {std::numeric_limits<int64_t>::min(), 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<uint32_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, LargerUnsignedToSmallerSignedInteger) {
  struct ValuePair {
    uint64_t from;
    int32_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<uint64_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<uint64_t>::max() / 2 + 42,
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<int32_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {42, 42},
      {0, 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<int32_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, SignedToUnsigned32BitInteger) {
  struct ValuePair {
    int32_t from;
    uint32_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<int32_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {42, 42},
      {0, 0},
      {-42, 0},
      {std::numeric_limits<int32_t>::min(), 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<uint32_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, UnsignedToSigned32BitInteger) {
  struct ValuePair {
    uint32_t from;
    int32_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<uint32_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<uint32_t>::max() / 2 + 42,
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<int32_t>::max(),
       std::numeric_limits<int32_t>::max()},
      {42, 42},
      {0, 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<int32_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, SignedToUnsigned64BitInteger) {
  struct ValuePair {
    int64_t from;
    uint64_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<int64_t>::max(),
       std::numeric_limits<int64_t>::max()},
      {42, 42},
      {0, 0},
      {-42, 0},
      {std::numeric_limits<int64_t>::min(), 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<uint64_t>(value_pair.from));
  }
}

TEST(SaturateCastTest, UnsignedToSigned64BitInteger) {
  struct ValuePair {
    uint64_t from;
    int64_t to;
  };
  constexpr ValuePair kValuePairs[] = {
      {std::numeric_limits<uint64_t>::max(),
       std::numeric_limits<int64_t>::max()},
      {std::numeric_limits<uint64_t>::max() / 2 + 42,
       std::numeric_limits<int64_t>::max()},
      {std::numeric_limits<int64_t>::max(),
       std::numeric_limits<int64_t>::max()},
      {42, 42},
      {0, 0},
  };
  for (const ValuePair& value_pair : kValuePairs) {
    EXPECT_EQ(value_pair.to, saturate_cast<int64_t>(value_pair.from));
  }
}

}  // namespace
}  // namespace openscreen
