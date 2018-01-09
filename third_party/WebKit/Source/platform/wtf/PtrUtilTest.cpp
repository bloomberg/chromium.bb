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

}  // namespace WTF
