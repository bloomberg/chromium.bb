// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/strings/string_util.h"
#include "remoting/base/capabilities.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct HasCapabilityTestData {
  const char* capabilities;
  const char* key;
  bool result;
};

struct IntersectTestData {
  const char* left;
  const char* right;
  const char* result;
};

}  // namespace

namespace remoting {

TEST(CapabilitiesTest, Empty) {
  // Expect that nothing can be found in an empty set.
  EXPECT_FALSE(HasCapability("", "a"));
  EXPECT_FALSE(HasCapability(" ", "a"));
  EXPECT_FALSE(HasCapability("  ", "a"));

  // Expect that nothing can be found in an empty set, event when the key is
  // empty.
  EXPECT_FALSE(HasCapability("", ""));
  EXPECT_FALSE(HasCapability(" ", ""));
  EXPECT_FALSE(HasCapability("  ", ""));
}

TEST(CapabilitiesTest, HasCapability) {
  HasCapabilityTestData data[] = {
    { "", "", false },
    { "a", "", false },
    { "a", "a", true },
    { "a a", "", false },
    { "a a", "a", true },
    { "a a", "z", false },
    { "a b", "", false },
    { "a b", "a", true },
    { "a b", "b", true },
    { "a b", "z", false },
    { "a b c", "", false },
    { "a b c", "a", true },
    { "a b c", "b", true },
    { "a b c", "z", false }
  };

  // Verify that HasCapability(|capabilities|, |key|) returns |result|.
  // |result|.
  for (size_t i = 0; i < arraysize(data); ++i) {
    std::vector<std::string> caps;
    Tokenize(data[i].capabilities, " ", &caps);

    do {
      EXPECT_EQ(HasCapability(JoinString(caps, " "), data[i].key),
                data[i].result);
    } while (std::next_permutation(caps.begin(), caps.end()));
  }
}

TEST(CapabilitiesTest, Intersect) {
  EXPECT_EQ(IntersectCapabilities("a", "a"), "a");

  IntersectTestData data[] = {
    { "", "", "" },
    { "a", "", "" },
    { "a", "a", "a" },
    { "a", "b", "" },
    { "a b", "", "" },
    { "a b", "a", "a" },
    { "a b", "b", "b" },
    { "a b", "z", "" },
    { "a b c", "a", "a" },
    { "a b c", "b", "b" },
    { "a b c", "a b", "a b" },
    { "a b c", "b a", "a b" },
    { "a b c", "z", "" }
  };

  // Verify that intersection of |right| with all permutations of |left| yields
  // |result|.
  for (size_t i = 0; i < arraysize(data); ++i) {
    std::vector<std::string> caps;
    Tokenize(data[i].left, " ", &caps);

    do {
      EXPECT_EQ(IntersectCapabilities(JoinString(caps, " "), data[i].right),
                data[i].result);
    } while (std::next_permutation(caps.begin(), caps.end()));
  }
}

}  // namespace remoting
