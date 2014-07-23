// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/testing/PrivateScriptTest.h"

#include "bindings/core/v8/PrivateScriptRunner.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PrivateScriptTest.h"
#include "core/testing/DummyPageHolder.h"

#include <gtest/gtest.h>

// PrivateScriptTest.js is available only in debug builds.
#ifndef NDEBUG
namespace blink {

namespace {

class PrivateScriptTestTest : public ::testing::Test {
public:
    PrivateScriptTestTest()
        : m_scope(v8::Isolate::GetCurrent())
        , m_dummyPageHolder(DummyPageHolder::create())
        , m_privateScriptTest(PrivateScriptTest::create(frame()))
    {
    }

    ~PrivateScriptTestTest()
    {
    }

    LocalFrame* frame() const { return &m_dummyPageHolder->frame(); }
    Document* document() const { return &m_dummyPageHolder->document(); }
    v8::Isolate* isolate() const { return m_scope.isolate(); }
    PrivateScriptTest* privateScriptTest() const { return m_privateScriptTest.get(); }

protected:
    V8TestingScope m_scope;
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    Persistent<PrivateScriptTest> m_privateScriptTest;
};

TEST_F(PrivateScriptTestTest, invokePrivateScriptMethodFromCPP)
{
    bool success;
    int result;
    success = V8PrivateScriptTest::addIntegerForPrivateScriptOnlyMethodImplementedInPrivateScript(frame(), privateScriptTest(), 100, 200, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result, 300);
}

TEST_F(PrivateScriptTestTest, invokePrivateScriptAttributeFromCPP)
{
    bool success;
    String result;
    success = V8PrivateScriptTest::stringAttributeForPrivateScriptOnlyAttributeGetterImplementedInPrivateScript(frame(), privateScriptTest(), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result, "yyy");
    success = V8PrivateScriptTest::stringAttributeForPrivateScriptOnlyAttributeSetterImplementedInPrivateScript(frame(), privateScriptTest(), "foo");
    EXPECT_TRUE(success);
    success = V8PrivateScriptTest::stringAttributeForPrivateScriptOnlyAttributeGetterImplementedInPrivateScript(frame(), privateScriptTest(), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result, "foo");
}

} // namespace

} // namespace blink
#endif
