// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/RefPtr.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringImpl.h"

namespace WTF {

TEST(RefPtrTest, Basic)
{
    RefPtr<StringImpl> string;
    EXPECT_TRUE(!string);
    string = StringImpl::create("test");
    EXPECT_TRUE(!!string);
    string.clear();
    EXPECT_TRUE(!string);
}

TEST(RefPtrTest, MoveAssignmentOperator)
{
    RefPtr<StringImpl> a = StringImpl::create("a");
    RefPtr<StringImpl> b = StringImpl::create("b");
    b = std::move(a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(!a);
}

} // namespace WTF
