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

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/DOMWrapperWorld.h"
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
    {
    }

    void SetUp()
    {
        m_scope = V8ExecutionScope::create(m_isolate);
        m_resolver = ScriptPromiseResolver::create(m_isolate);
        m_promise = m_resolver->promise();
    }

    void TearDown()
    {
        m_resolver = nullptr;
        m_promise.clear();
        m_scope.clear();
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
        ASSERT(!m_promise.isEmpty());
        return m_promise.v8Value().As<v8::Object>();
    }

protected:
    v8::Isolate* m_isolate;
    RefPtr<ScriptPromiseResolver> m_resolver;
    ScriptPromise m_promise;
private:
    OwnPtr<V8ExecutionScope> m_scope;
};

TEST_F(ScriptPromiseResolverTest, initialState)
{
    if (RuntimeEnabledFeatures::scriptPromiseOnV8PromiseEnabled())
        return;
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());
}

TEST_F(ScriptPromiseResolverTest, resolve)
{
    if (RuntimeEnabledFeatures::scriptPromiseOnV8PromiseEnabled())
        return;
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->resolve(ScriptValue(v8::Integer::New(m_isolate, 3), m_isolate));

    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, reject)
{
    if (RuntimeEnabledFeatures::scriptPromiseOnV8PromiseEnabled())
        return;
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->reject(ScriptValue(v8::Integer::New(m_isolate, 3), m_isolate));

    EXPECT_EQ(V8PromiseCustom::Rejected, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, resolveOverResolve)
{
    if (RuntimeEnabledFeatures::scriptPromiseOnV8PromiseEnabled())
        return;
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->resolve(ScriptValue(v8::Integer::New(m_isolate, 3), m_isolate));

    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());

    m_resolver->resolve(ScriptValue(v8::Integer::New(m_isolate, 4), m_isolate));
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

TEST_F(ScriptPromiseResolverTest, rejectOverResolve)
{
    if (RuntimeEnabledFeatures::scriptPromiseOnV8PromiseEnabled())
        return;
    EXPECT_EQ(V8PromiseCustom::Pending, state());
    EXPECT_TRUE(result()->IsUndefined());

    m_resolver->resolve(ScriptValue(v8::Integer::New(m_isolate, 3), m_isolate));

    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());

    m_resolver->reject(ScriptValue(v8::Integer::New(m_isolate, 4), m_isolate));
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state());
    ASSERT_TRUE(result()->IsNumber());
    EXPECT_EQ(3, result().As<v8::Integer>()->Value());
}

} // namespace

} // namespace WebCore
