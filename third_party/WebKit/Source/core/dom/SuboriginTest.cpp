// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Suborigin.h"

#include "core/dom/Document.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SuboriginTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        m_document = Document::create();
    }

    RefPtrWillBePersistent<Document> m_document;
};

TEST_F(SuboriginTest, ParseValidHeaders)
{
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "foo"));
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "  foo  "));
    EXPECT_EQ("Foo", SuboriginPolicy::parseSuboriginName(*m_document, "Foo"));
    EXPECT_EQ("FOO", SuboriginPolicy::parseSuboriginName(*m_document, "FOO"));
    EXPECT_EQ("f0o", SuboriginPolicy::parseSuboriginName(*m_document, "f0o"));
    EXPECT_EQ("42", SuboriginPolicy::parseSuboriginName(*m_document, "42"));
    EXPECT_EQ("foo-bar", SuboriginPolicy::parseSuboriginName(*m_document, "foo-bar"));
    EXPECT_EQ("-foobar", SuboriginPolicy::parseSuboriginName(*m_document, "-foobar"));
    EXPECT_EQ("foobar-", SuboriginPolicy::parseSuboriginName(*m_document, "foobar-"));

    // Mulitple headers should only give the first name
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "foo, bar"));
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "foo,bar"));
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "foo , bar"));
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "foo,"));

    // If the second value in multiple headers is invalid, it is still ignored.
    EXPECT_EQ("foo", SuboriginPolicy::parseSuboriginName(*m_document, "foo, @bar"));
}

TEST_F(SuboriginTest, ParseInvalidHeaders)
{
    // Single header, invalid value
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, ""));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "foo bar"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "'foobar'"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "foobar'"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "foo'bar"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "foob@r"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "foo bar"));

    // Multiple headers, invalid value(s)
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, ", bar"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, ","));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "f@oo, bar"));
    EXPECT_EQ(String(), SuboriginPolicy::parseSuboriginName(*m_document, "f@oo, b@r"));
}

} // namespace blink
