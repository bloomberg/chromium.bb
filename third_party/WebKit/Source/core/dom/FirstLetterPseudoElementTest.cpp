// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/FirstLetterPseudoElement.h"

#include <gtest/gtest.h>

namespace blink {

class FirstLetterPseudoElementTest : public ::testing::Test {};

TEST_F(FirstLetterPseudoElementTest, DoesNotBreakEmoji) {
  const UChar emoji[] = {0xD83D, 0xDE31, 0};
  EXPECT_EQ(2u, FirstLetterPseudoElement::FirstLetterLength(emoji));
}

}  // namespace blink
