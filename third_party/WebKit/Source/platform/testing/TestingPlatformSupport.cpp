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

#include "platform/testing/TestingPlatformSupport.h"

namespace blink {

TestingDiscardableMemory::TestingDiscardableMemory(size_t size) : m_data(size), m_isLocked(true)
{
}

TestingDiscardableMemory::~TestingDiscardableMemory()
{
}

bool TestingDiscardableMemory::lock()
{
    ASSERT(!m_isLocked);
    m_isLocked = true;
    return false;
}

void* TestingDiscardableMemory::data()
{
    ASSERT(m_isLocked);
    return m_data.data();
}

void TestingDiscardableMemory::unlock()
{
    ASSERT(m_isLocked);
    m_isLocked = false;
    // Force eviction to catch clients not correctly checking the return value of lock().
    memset(m_data.data(), 0, m_data.size());
}

WebMemoryAllocatorDump* TestingDiscardableMemory::createMemoryAllocatorDump(const WebString& name, WebProcessMemoryDump* dump) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

TestingPlatformSupport::TestingPlatformSupport()
    : TestingPlatformSupport(TestingPlatformSupport::Config())
{
}

TestingPlatformSupport::TestingPlatformSupport(const Config& config)
    : m_config(config)
    , m_oldPlatform(Platform::current())
{
    Platform::initialize(this);
}

TestingPlatformSupport::~TestingPlatformSupport()
{
    Platform::initialize(m_oldPlatform);
}

WebDiscardableMemory* TestingPlatformSupport::allocateAndLockDiscardableMemory(size_t bytes)
{
    return !m_config.hasDiscardableMemorySupport ? 0 : new TestingDiscardableMemory(bytes);
}

const unsigned char* TestingPlatformSupport::getTraceCategoryEnabledFlag(const char* categoryName)
{
    static const unsigned char tracingIsDisabled = 0;
    return &tracingIsDisabled;
}

WebString TestingPlatformSupport::defaultLocale()
{
    return WebString::fromUTF8("en-US");
}

WebCompositorSupport* TestingPlatformSupport::compositorSupport()
{
    return m_config.compositorSupport;
}

WebUnitTestSupport* TestingPlatformSupport::unitTestSupport()
{
    return m_oldPlatform ? m_oldPlatform->unitTestSupport() : nullptr;
}

WebThread* TestingPlatformSupport::currentThread()
{
    return m_oldPlatform ? m_oldPlatform->currentThread() : nullptr;
}

class TestingPlatformMockWebTaskRunner : public WebTaskRunner {
    WTF_MAKE_NONCOPYABLE(TestingPlatformMockWebTaskRunner);
public:
    explicit TestingPlatformMockWebTaskRunner(Deque<OwnPtr<WebTaskRunner::Task>>* tasks) : m_tasks(tasks) { }
    ~TestingPlatformMockWebTaskRunner() override { }

    void postTask(const WebTraceLocation&, Task* task) override
    {
        m_tasks->append(adoptPtr(task));
    }

    void postDelayedTask(const WebTraceLocation&, Task*, double delayMs) override
    {
        ASSERT_NOT_REACHED();
    }

    WebTaskRunner* clone() override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

private:
    Deque<OwnPtr<WebTaskRunner::Task>>* m_tasks; // NOT OWNED
};

// TestingPlatformMockScheduler definition:

TestingPlatformMockScheduler::TestingPlatformMockScheduler()
    : m_mockWebTaskRunner(adoptPtr(new TestingPlatformMockWebTaskRunner(&m_tasks))) { }

TestingPlatformMockScheduler::~TestingPlatformMockScheduler() { }

WebTaskRunner* TestingPlatformMockScheduler::loadingTaskRunner()
{
    return m_mockWebTaskRunner.get();
}

WebTaskRunner* TestingPlatformMockScheduler::timerTaskRunner()
{
    return m_mockWebTaskRunner.get();
}

void TestingPlatformMockScheduler::runSingleTask()
{
    if (m_tasks.isEmpty())
        return;
    m_tasks.takeFirst()->run();
}

void TestingPlatformMockScheduler::runAllTasks()
{
    while (!m_tasks.isEmpty())
        m_tasks.takeFirst()->run();
}

class TestingPlatformMockWebThread : public WebThread {
    WTF_MAKE_NONCOPYABLE(TestingPlatformMockWebThread);
public:
    TestingPlatformMockWebThread() : m_mockWebScheduler(adoptPtr(new TestingPlatformMockScheduler)) { }
    ~TestingPlatformMockWebThread() override { }

    WebTaskRunner* taskRunner() override
    {
        return m_mockWebScheduler->timerTaskRunner();
    }

    bool isCurrentThread() const override
    {
        ASSERT_NOT_REACHED();
        return true;
    }

    WebScheduler* scheduler() const override
    {
        return m_mockWebScheduler.get();
    }

    TestingPlatformMockScheduler* mockWebScheduler()
    {
        return m_mockWebScheduler.get();
    }

private:
    OwnPtr<TestingPlatformMockScheduler> m_mockWebScheduler;
};

// TestingPlatformSupportWithMockScheduler definition:

TestingPlatformSupportWithMockScheduler::TestingPlatformSupportWithMockScheduler()
    : m_mockWebThread(adoptPtr(new TestingPlatformMockWebThread())) { }

TestingPlatformSupportWithMockScheduler::TestingPlatformSupportWithMockScheduler(const Config& config)
    : TestingPlatformSupport(config)
    , m_mockWebThread(adoptPtr(new TestingPlatformMockWebThread())) { }

TestingPlatformSupportWithMockScheduler::~TestingPlatformSupportWithMockScheduler() { }

WebThread* TestingPlatformSupportWithMockScheduler::currentThread()
{
    return m_mockWebThread.get();
}

TestingPlatformMockScheduler* TestingPlatformSupportWithMockScheduler::mockWebScheduler()
{
    return m_mockWebThread->mockWebScheduler();
}

} // namespace blink
