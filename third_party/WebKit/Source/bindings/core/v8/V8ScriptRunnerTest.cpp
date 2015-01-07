// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/V8ScriptRunner.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/fetch/ScriptResource.h"
#include "platform/heap/Handle.h"
#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

namespace {

class V8ScriptRunnerTest : public ::testing::Test {
public:
    V8ScriptRunnerTest() : m_scope(v8::Isolate::GetCurrent()) { }
    virtual ~V8ScriptRunnerTest() { }

    virtual void SetUp() override
    {
        // To trick various layers of caching, increment a counter for each
        // test and use it in code(), fielname() and url().
        counter++;
    }

    virtual void TearDown() override
    {
        m_resourceRequest.clear();
        m_resource.clear();
    }

    v8::Isolate* isolate() const
    {
        return m_scope.isolate();
    }
    WTF::String code() const
    {
        // Simple function for testing. Note:
        // - Add counter to trick V8 code cache.
        // - Pad counter to 1000 digits, to trick minimal cacheability threshold.
        return WTF::String::format("a = function() { 1 + 1; } // %01000d\n", counter);
    }
    WTF::String filename() const
    {
        return WTF::String::format("whatever%d.js", counter);
    }
    WTF::String url() const
    {
        return WTF::String::format("http://bla.com/bla%d", counter);
    }
    unsigned tagForParserCache(Resource* resource) const
    {
        return V8ScriptRunner::tagForParserCache(resource);
    }
    unsigned tagForCodeCache(Resource* resource) const
    {
        return V8ScriptRunner::tagForCodeCache(resource);
    }

    bool compileScript(V8CacheOptions cacheOptions)
    {
        return !V8ScriptRunner::compileScript(
            v8String(isolate(), code()), filename(), WTF::TextPosition(),
            m_resource.get(), 0, isolate(), NotSharableCrossOrigin, cacheOptions)
            .IsEmpty();
    }

    void setEmptyResource()
    {
        m_resourceRequest = WTF::adoptPtr(new ResourceRequest);
        m_resource = ScriptResource::create(*m_resourceRequest.get(), "UTF-8");
    }

    void setResource()
    {
        m_resourceRequest = WTF::adoptPtr(new ResourceRequest(url()));
        m_resource = ScriptResource::create(*m_resourceRequest.get(), "UTF-8");
    }

protected:
    WTF::OwnPtr<ResourceRequest> m_resourceRequest;
    OwnPtrWillBePersistent<ScriptResource> m_resource;
    V8TestingScope m_scope;

    static int counter;
};

int V8ScriptRunnerTest::counter = 0;

TEST_F(V8ScriptRunnerTest, resourcelessShouldPass)
{
    EXPECT_TRUE(compileScript(V8CacheOptionsNone));
    EXPECT_TRUE(compileScript(V8CacheOptionsParseMemory));
    EXPECT_TRUE(compileScript(V8CacheOptionsParse));
    EXPECT_TRUE(compileScript(V8CacheOptionsCode));
}

TEST_F(V8ScriptRunnerTest, emptyResourceDoesNothing)
{
    setEmptyResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsDefault));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));

    EXPECT_TRUE(compileScript(V8CacheOptionsParse));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));

    EXPECT_TRUE(compileScript(V8CacheOptionsCode));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));
}

TEST_F(V8ScriptRunnerTest, parseMemoryOption)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsParseMemory));
    EXPECT_TRUE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));
    // The cached data is associated with the encoding.
    ResourceRequest request(url());
    OwnPtrWillBeRawPtr<ScriptResource> anotherResource = ScriptResource::create(request, "UTF-16");
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(anotherResource.get())));
}

TEST_F(V8ScriptRunnerTest, parseOption)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsParse));
    EXPECT_TRUE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));
    // The cached data is associated with the encoding.
    ResourceRequest request(url());
    OwnPtrWillBeRawPtr<ScriptResource> anotherResource = ScriptResource::create(request, "UTF-16");
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(anotherResource.get())));
}

TEST_F(V8ScriptRunnerTest, codeOption)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsCode));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_TRUE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));
    // The cached data is associated with the encoding.
    ResourceRequest request(url());
    OwnPtrWillBeRawPtr<ScriptResource> anotherResource = ScriptResource::create(request, "UTF-16");
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(anotherResource.get())));
}

TEST_F(V8ScriptRunnerTest, codeCompressedOptions)
{
    setResource();
    EXPECT_TRUE(compileScript(V8CacheOptionsCodeCompressed));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForParserCache(m_resource.get())));
    EXPECT_FALSE(m_resource->cachedMetadata(tagForCodeCache(m_resource.get())));
}

} // namespace

} // namespace blink
