// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/PtrUtil.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(WTF_PtrUtilTest, canWrapUnique) {
  auto char_ptr = new char;
  auto wrapped_char_ptr = WrapUnique(char_ptr);

  ASSERT_TRUE(wrapped_char_ptr.get());
}

TEST(WTF_PtrUtilTest, canWrapUniqueArray) {
  constexpr size_t kBufferSize = 20;
  auto char_array = new char[kBufferSize];
  auto wrapped_char_array = WrapArrayUnique(char_array);

  ASSERT_TRUE(wrapped_char_array.get());
}

TEST(WTF_PtrUtilTest, canMakeUnique) {
  auto char_ptr = MakeUnique<char>();

  ASSERT_TRUE(char_ptr.get());
}

TEST(WTF_PtrUtilTest, canMakeUniqueArray) {
  constexpr size_t kBufferSize = 20;
  auto char_array = MakeUnique<char[]>(kBufferSize);

  ASSERT_TRUE(char_array.get());
}

struct EmptyStruct {};

TEST(WTF_PtrUtilTest, canMakeUniqueArrayOfStructs) {
  constexpr size_t kBufferSize = 20;
  auto struct_array = MakeUnique<EmptyStruct[]>(kBufferSize);

  ASSERT_TRUE(struct_array.get());
}

}  // namespace WTF
