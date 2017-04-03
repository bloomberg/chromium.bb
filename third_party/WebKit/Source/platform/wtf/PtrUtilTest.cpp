// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/PtrUtil.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(WTF_PtrUtilTest, canWrapUnique) {
  auto charPtr = new char;
  auto wrappedCharPtr = wrapUnique(charPtr);

  ASSERT_TRUE(wrappedCharPtr.get());
}

TEST(WTF_PtrUtilTest, canWrapUniqueArray) {
  constexpr size_t bufferSize = 20;
  auto charArray = new char[bufferSize];
  auto wrappedCharArray = wrapArrayUnique(charArray);

  ASSERT_TRUE(wrappedCharArray.get());
}

TEST(WTF_PtrUtilTest, canMakeUnique) {
  auto charPtr = makeUnique<char>();

  ASSERT_TRUE(charPtr.get());
}

TEST(WTF_PtrUtilTest, canMakeUniqueArray) {
  constexpr size_t bufferSize = 20;
  auto charArray = makeUnique<char[]>(bufferSize);

  ASSERT_TRUE(charArray.get());
}
}
