/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#include "platform/LifecycleContext.h"

#include "platform/LifecycleNotifier.h"
#include "platform/heap/Handle.h"
#include <gtest/gtest.h>

using namespace blink;

namespace blink {

class DummyContext : public LifecycleContext<DummyContext> {
public:
    PassOwnPtr<LifecycleNotifier<DummyContext>> createLifecycleNotifier()
    {
        return LifecycleNotifier<DummyContext>::create(this);
    }
    LifecycleNotifier<DummyContext>& lifecycleNotifier()
    {
        return static_cast<LifecycleNotifier<DummyContext>&>(LifecycleContext<DummyContext>::lifecycleNotifier());
    }

private:
    OwnPtr<LifecycleNotifier<DummyContext>> m_lifecycleNotifier;
};

template<> void observerContext(DummyContext* context, LifecycleObserver<DummyContext>* observer)
{
    context->wasObservedBy(observer);
}

template<> void unobserverContext(DummyContext* context, LifecycleObserver<DummyContext>* observer)
{
    context->wasUnobservedBy(observer);
}

class TestingObserver final : public GarbageCollectedFinalized<TestingObserver>, public LifecycleObserver<DummyContext> {
public:
    explicit TestingObserver(DummyContext* context)
        : LifecycleObserver<DummyContext>(context)
        , m_contextDestroyedCalled(false)
    { }

    virtual void contextDestroyed() override
    {
        LifecycleObserver<DummyContext>::contextDestroyed();
        m_contextDestroyedCalled = true;
    }

    void trace(Visitor*) { }

    bool m_contextDestroyedCalled;

    void unobserve() { observeContext(0); }
};

TEST(LifecycleContextTest, shouldObserveContextDestroyed)
{
    OwnPtr<DummyContext> context = adoptPtr(new DummyContext());
    TestingObserver* observer = new TestingObserver(context.get());

    EXPECT_EQ(observer->lifecycleContext(), context.get());
    EXPECT_FALSE(observer->m_contextDestroyedCalled);
    context->notifyContextDestroyed();
    context.clear();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_TRUE(observer->m_contextDestroyedCalled);
}

TEST(LifecycleContextTest, shouldNotObserveContextDestroyedIfUnobserve)
{
    OwnPtr<DummyContext> context = adoptPtr(new DummyContext());
    TestingObserver* observer = new TestingObserver(context.get());
    observer->unobserve();
    context->notifyContextDestroyed();
    context.clear();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_FALSE(observer->m_contextDestroyedCalled);
}

}
