/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "bindings/v8/ScriptPromise.h"

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8PromiseCustom.h"

#include <gtest/gtest.h>
#include <v8.h>

namespace WebCore {

namespace {

void callback(const v8::FunctionCallbackInfo<v8::Value>& info) { }

class ScriptPromiseTest : public testing::Test {
public:
    ScriptPromiseTest()
        : m_isolate(v8::Isolate::GetCurrent())
    {
        m_scope = V8ExecutionScope::create(m_isolate);
    }

    ~ScriptPromiseTest()
    {
        // FIXME: We put this statement here to clear an exception from the isolate.
        createClosure(callback, v8::Undefined(m_isolate), m_isolate);
    }

    V8PromiseCustom::PromiseState state(ScriptPromise promise)
    {
        return V8PromiseCustom::getState(V8PromiseCustom::getInternal(promise.v8Value().As<v8::Object>()));
    }

protected:
    v8::Isolate* m_isolate;

private:
    OwnPtr<V8ExecutionScope> m_scope;
};

TEST_F(ScriptPromiseTest, constructFromNonPromise)
{
    v8::TryCatch trycatch;
    ScriptPromise promise(v8::Undefined(m_isolate), m_isolate);
    ASSERT_TRUE(trycatch.HasCaught());
    ASSERT_TRUE(promise.hasNoValue());
}

TEST_F(ScriptPromiseTest, castPromise)
{
    ScriptPromise promise = ScriptPromiseResolver::create(m_isolate)->promise();
    ScriptPromise newPromise = ScriptPromise::cast(ScriptValue(promise.v8Value(), m_isolate));

    ASSERT_FALSE(promise.hasNoValue());
    EXPECT_EQ(V8PromiseCustom::Pending, state(promise));
    EXPECT_EQ(promise.v8Value(), newPromise.v8Value());
}

TEST_F(ScriptPromiseTest, castNonPromise)
{
    ScriptValue value = ScriptValue(v8String(m_isolate, "hello"), m_isolate);
    ScriptPromise promise1 = ScriptPromise::cast(ScriptValue(value.v8Value(), m_isolate));
    ScriptPromise promise2 = ScriptPromise::cast(ScriptValue(value.v8Value(), m_isolate));

    ASSERT_FALSE(promise1.hasNoValue());
    ASSERT_FALSE(promise2.hasNoValue());

    ASSERT_TRUE(V8PromiseCustom::isPromise(promise1.v8Value(), m_isolate));
    ASSERT_TRUE(V8PromiseCustom::isPromise(promise2.v8Value(), m_isolate));

    EXPECT_EQ(V8PromiseCustom::Fulfilled, state(promise1));
    EXPECT_EQ(V8PromiseCustom::Fulfilled, state(promise2));
    EXPECT_NE(promise1.v8Value(), promise2.v8Value());
}

} // namespace

} // namespace WebCore
