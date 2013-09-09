/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/ScriptPromiseResolver.h"

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8PromiseCustom.h"

#include <gtest/gtest.h>
#include <v8.h>

namespace WebCore {

namespace {

class ScriptPromiseResolverTest : public testing::Test {
public:
    ScriptPromiseResolverTest()
        : m_isolate(v8::Isolate::GetCurrent())
        , m_handleScope(m_isolate)
        , m_context(v8::Context::New(m_isolate))
        , m_contextScope(m_context.newLocal(m_isolate))
    {
    }

    void SetUp()
    {
        m_perContextData = V8PerContextData::create(m_context.newLocal(m_isolate));
        m_perContextData->init();
        m_resolver = ScriptPromiseResolver::create();
        m_promise = m_resolver->promise();
    }

    void TearDown()
    {
        m_resolver = 0;
        m_promise.clear();
        m_perContextData.clear();
    }

    V8PromiseCustom::PromiseState state()
    {
        return V8PromiseCustom::getState(V8PromiseCustom::getInternal(promise()));
    }

    v8::Local<v8::Value> result()
    {
        return V8PromiseCustom::getInternal(promise())->GetInternalField(V8PromiseCustom::InternalResultIndex);
    }

    v8::Local<v8::Object> promise()
    {
        ASSERT(!m_promise.hasNoValue());
        return m_promise.v8Value().As<v8::Object>();
    }

protected:
    v8::Isolate* m_isolate;
    v8::HandleScope m_handleScope;
    ScopedPersistent<v8::Context> m_context;
    v8::Context::Scope m_contextScope;
    RefPtr<ScriptPromiseResolver> m_resolver;
    ScriptPromise m_promise;
    OwnPtr<V8PerContextData> m_perContextData;
};

TEST_F(ScriptPromiseResolverTest, initialState)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());
}

TEST_F(ScriptPromiseResolverTest, fulfill)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->fulfill(ScriptValue(v8::Integer::New(3)));

    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, resolve)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->resolve(ScriptValue(v8::Integer::New(3)));

    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, reject)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->reject(ScriptValue(v8::Integer::New(3)));

    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Rejected, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, fulfillOverFulfill)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->fulfill(ScriptValue(v8::Integer::New(3)));

    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());

    m_resolver->fulfill(ScriptValue(v8::Integer::New(4)));
    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, rejectOverFulfill)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->fulfill(ScriptValue(v8::Integer::New(3)));

    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());

    m_resolver->reject(ScriptValue(v8::Integer::New(4)));
    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, detach)
{
    EXPECT_TRUE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->detach();

    EXPECT_FALSE(m_resolver->isPending());
    EXPECT_EQ(V8PromiseCustom::Rejected, state());
    EXPECT_TRUE(result()->IsUndefined());
}

} // namespace

} // namespace WebCore
