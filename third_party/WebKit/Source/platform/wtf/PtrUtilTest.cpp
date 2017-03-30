// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/PtrUtil.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(WTF_PtrUtilTest, canMakeUniqueArray) {
  constexpr size_t bufferSize = 20;
  auto charArray = WTF::makeUnique<char[]>(bufferSize);

  ASSERT_TRUE(charArray.get());
}
