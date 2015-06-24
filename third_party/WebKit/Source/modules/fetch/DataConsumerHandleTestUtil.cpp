// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"

#include "bindings/core/v8/DOMWrapperWorld.h"

namespace blink {

DataConsumerHandleTestUtil::Thread::Thread(const char* name, InitializationPolicy initializationPolicy)
    : m_thread(WebThreadSupportingGC::create(name))
    , m_initializationPolicy(initializationPolicy)
    , m_waitableEvent(adoptPtr(Platform::current()->createWaitableEvent()))
{
    m_thread->postTask(FROM_HERE, new Task(threadSafeBind(&Thread::initialize, AllowCrossThreadAccess(this))));
    m_waitableEvent->wait();
}

DataConsumerHandleTestUtil::Thread::~Thread()
{
    m_thread->postTask(FROM_HERE, new Task(threadSafeBind(&Thread::shutdown, AllowCrossThreadAccess(this))));
    m_waitableEvent->wait();
}

void DataConsumerHandleTestUtil::Thread::initialize()
{
    if (m_initializationPolicy >= ScriptExecution) {
        m_isolateHolder = adoptPtr(new gin::IsolateHolder());
        isolate()->Enter();
    }
    m_thread->initialize();
    if (m_initializationPolicy >= ScriptExecution) {
        v8::HandleScope handleScope(isolate());
        m_scriptState = ScriptState::create(v8::Context::New(isolate()), DOMWrapperWorld::create(isolate()));
    }
    if (m_initializationPolicy >= WithExecutionContext) {
        m_executionContext = adoptRefWillBeNoop(new NullExecutionContext());
    }
    m_waitableEvent->signal();
}

void DataConsumerHandleTestUtil::Thread::shutdown()
{
    m_executionContext = nullptr;
    m_scriptState = nullptr;
    m_thread->shutdown();
    if (m_isolateHolder) {
        isolate()->Exit();
        m_isolateHolder = nullptr;
    }
    m_waitableEvent->signal();
}

} // namespace blink
