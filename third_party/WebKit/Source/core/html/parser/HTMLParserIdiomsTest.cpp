// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLParserIdioms.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(HTMLParserIdiomsTest, ParseHTMLListOfFloatingPointNumbers_null)
{
    Vector<double> numbers = parseHTMLListOfFloatingPointNumbers(nullAtom);
    EXPECT_EQ(numbers.size(), 0u);
}

} // namespace

} // namespace blink
