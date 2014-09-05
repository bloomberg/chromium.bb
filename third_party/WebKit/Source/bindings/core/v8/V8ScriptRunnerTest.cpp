// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/V8ScriptRunner.h"

#include "core/fetch/ScriptResource.h"
#include "platform/heap/Handle.h"
#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

namespace {

class V8ScriptRunnerTest : public ::testing::Test {
public:
    V8ScriptRunnerTest() { }
    virtual ~V8ScriptRunnerTest() { }

    static void SetUpTestCase()
    {
        cacheTagParser = StringHash::hash(v8::V8::GetVersion()) * 2;
        cacheTagCode = cacheTagParser + 1;
    }

    virtual void SetUp() OVERRIDE
    {
        // To trick various layers of caching, increment a counter for each
        // test and use it in code(), fielname() and url().
        counter++;
    }

    virtual void TearDown() OVERRIDE
    {
        m_resourceRequest.clear();
        m_resource.clear();
    }

    v8::Isolate* isolate()
    {
        return v8::Isolate::GetCurrent();
    }
    v8::Local<v8::String> v8String(const char* value)
    {
        return v8::String::NewFromOneByte(
            isolate(), reinterpret_cast<const uint8_t*>(value));
    }
    v8::Local<v8::String> v8String(const WTF::String& value)
    {
        return v8String(value.ascii().data());
    }
    WTF::String code()
    {
        // Simple function for testing. Note:
        // - Add counter to trick V8 code cache.
        // - Pad counter to 1000 digits, to trick minimal cacheability threshold.
        return WTF::String::format("a = function() { 1 + 1; } // %01000d\n", counter);
    }
    WTF::String filename()
    {
        return WTF::String::format("whatever%d.js", counter);
    }
    WTF::String url()
    {
        return WTF::String::format("http://bla.com/bla%d", counter);
    }

    bool compileScript(V8CacheOptions cacheOptions)
    {
        v8::HandleScope handleScope(isolate());
        v8::Local<v8::Context> context = v8::Context::New(isolate());
        v8::Context::Scope contextScope(context);
        return !V8ScriptRunner::compileScript(
            v8String(code()), filename(), WTF::TextPosition(), m_resource.get(),
            isolate(), NotSharableCrossOrigin, cacheOptions)
            .IsEmpty();
    }

    void setEmptyResource()
    {
        m_resourceRequest = WTF::adoptPtr(new ResourceRequest);
        m_resource = adoptPtrWillBeNoop(new ScriptResource(*m_resourceRequest.get(), "text/utf-8"));
    }

    void setResource()
    {
        m_resourceRequest = WTF::adoptPtr(new ResourceRequest(url()));
        m_resource = adoptPtrWillBeNoop(new ScriptResource(*m_resourceRequest.get(), "text/utf-8"));
    }

protected:
    WTF::OwnPtr<ResourceRequest> m_resourceRequest;
    OwnPtrWillBePersistent<ScriptResource> m_resource;

    static unsigned cacheTagParser;
    static unsigned cacheTagCode;
    static int counter;
};

unsigned V8ScriptRunnerTest::cacheTagParser = 0;
unsigned V8ScriptRunnerTest::cacheTagCode = 0;
int V8ScriptRunnerTest::counter = 0;

TEST_F(V8ScriptRunnerTest, resourcelessShouldPass)
{
    EXPECT_TRUE(compileScript(V8CacheOptionsOff));
    EXPECT_TRUE(compileScript(V8CacheOptionsParse));
    EXPECT_TRUE(compileScript(V8CacheOptionsCode));
}

TEST_F(V8ScriptRunnerTest, emptyResourceDoesNothing)
{
    setEmptyResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsOff));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagParser));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagCode));

    EXPECT_TRUE(compileScript(V8CacheOptionsParse));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagParser));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagCode));

    EXPECT_TRUE(compileScript(V8CacheOptionsCode));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagParser));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagCode));
}

TEST_F(V8ScriptRunnerTest, defaultOptions)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsOff));
    EXPECT_TRUE(m_resource->cachedMetadata(cacheTagParser));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagCode));
}

TEST_F(V8ScriptRunnerTest, parseOptions)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsParse));
    EXPECT_TRUE(m_resource->cachedMetadata(cacheTagParser));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagCode));
}

TEST_F(V8ScriptRunnerTest, codeOptions)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsCode));
    EXPECT_FALSE(m_resource->cachedMetadata(cacheTagParser));

    // TODO(vogelheim): Code caching is presently still disabled.
    //                  Enable EXPECT when code caching lands.
    // EXPECT_TRUE(m_resource->cachedMetadata(cacheTagCode));
}

} // namespace

} // namespace blink
