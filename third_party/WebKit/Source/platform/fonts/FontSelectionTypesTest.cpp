// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/Font.h"

#include "platform/fonts/FontSelectionTypes.h"
#include "platform/wtf/HashSet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontSelectionTypesTest, HashCollisions) {
  std::vector<int> weights = {100, 200, 300, 400, 500, 600, 700, 800, 900};
  std::vector<float> slopes = {-90, -67.5, -30, -20,  -10, 0,
                               10,  20,    30,  67.5, 90};
  std::vector<float> widths = {50, 67.5, 75, 100, 125, 150, 167.5, 175, 200};

  HashSet<unsigned> hashes;
  for (auto weight : weights) {
    for (auto slope : slopes) {
      for (auto width : widths) {
        FontSelectionRequest request = FontSelectionRequest(
            FontSelectionValue(weight), FontSelectionValue(width),
            FontSelectionValue(slope));
        ASSERT_FALSE(hashes.Contains(request.GetHash()));
        ASSERT_TRUE(hashes.insert(request.GetHash()).is_new_entry);
      }
    }
  }
  ASSERT_EQ(hashes.size(), weights.size() * slopes.size() * widths.size());
}

}  // namespace blink
