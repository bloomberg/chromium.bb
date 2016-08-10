// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSResourceValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSResourceValue : public CSSResourceValue {
public:
    FakeCSSResourceValue(Resource::Status status)
        : m_status(status)
    {
    }

    CSSValue* toCSSValue() const override { return nullptr; }

    Resource::Status status() const override { return m_status; }

private:
    Resource::Status m_status;
};


TEST(CSSResourceValueTest, TestStatus)
{
    EXPECT_EQ((new FakeCSSResourceValue(Resource::NotStarted))->state(), "unloaded");
    EXPECT_EQ((new FakeCSSResourceValue(Resource::Pending))->state(), "loading");
    EXPECT_EQ((new FakeCSSResourceValue(Resource::Cached))->state(), "loaded");
    EXPECT_EQ((new FakeCSSResourceValue(Resource::LoadError))->state(), "error");
    EXPECT_EQ((new FakeCSSResourceValue(Resource::DecodeError))->state(), "error");
}

} // namespace

} // namespace blink
