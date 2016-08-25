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

#include "base/command_line.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/icu_test_util.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "platform/EventTracer.h"
#include "platform/HTTPNames.h"
#include "platform/heap/Heap.h"
#include "wtf/CryptographicallyRandomNumber.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/WTF.h"
#include "wtf/allocator/Partitions.h"
#include <memory>

namespace blink {

namespace {

double dummyCurrentTime()
{
    return 0.0;
}

class DummyThread final : public blink::WebThread {
public:
    bool isCurrentThread() const override { return true; }
    blink::WebScheduler* scheduler() const override { return nullptr; }
};

} // namespace

TestingPlatformSupport::TestingPlatformSupport()
    : TestingPlatformSupport(TestingPlatformSupport::Config())
{
}

TestingPlatformSupport::TestingPlatformSupport(const Config& config)
    : m_config(config)
    , m_oldPlatform(Platform::current())
{
    ASSERT(m_oldPlatform);
    Platform::setCurrentPlatformForTesting(this);
}

TestingPlatformSupport::~TestingPlatformSupport()
{
    Platform::setCurrentPlatformForTesting(m_oldPlatform);
}

WebString TestingPlatformSupport::defaultLocale()
{
    return WebString::fromUTF8("en-US");
}

WebCompositorSupport* TestingPlatformSupport::compositorSupport()
{
    return m_config.compositorSupport;
}

WebThread* TestingPlatformSupport::currentThread()
{
    return m_oldPlatform ? m_oldPlatform->currentThread() : nullptr;
}

class TestingPlatformMockWebTaskRunner : public WebTaskRunner {
    WTF_MAKE_NONCOPYABLE(TestingPlatformMockWebTaskRunner);
public:
    explicit TestingPlatformMockWebTaskRunner(Deque<std::unique_ptr<WebTaskRunner::Task>>* tasks) : m_tasks(tasks) { }
    ~TestingPlatformMockWebTaskRunner() override { }

    void postTask(const WebTraceLocation&, Task* task) override
    {
        m_tasks->append(wrapUnique(task));
    }

    void postDelayedTask(const WebTraceLocation&, Task*, double delayMs) override
    {
        NOTREACHED();
    }

    bool runsTasksOnCurrentThread() override
    {
        NOTREACHED();
        return true;
    }

    std::unique_ptr<WebTaskRunner> clone() override
    {
        return WTF::wrapUnique(new TestingPlatformMockWebTaskRunner(m_tasks));
    }

    double virtualTimeSeconds() const override
    {
        NOTREACHED();
        return 0.0;
    }

    double monotonicallyIncreasingVirtualTimeSeconds() const override
    {
        NOTREACHED();
        return 0.0;
    }

    base::SingleThreadTaskRunner* taskRunner() override
    {
        NOTREACHED();
        return nullptr;
    }

private:
    Deque<std::unique_ptr<WebTaskRunner::Task>>* m_tasks; // NOT OWNED
};

// TestingPlatformMockScheduler definition:

TestingPlatformMockScheduler::TestingPlatformMockScheduler()
    : m_mockWebTaskRunner(wrapUnique(new TestingPlatformMockWebTaskRunner(&m_tasks))) { }

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
    TestingPlatformMockWebThread() : m_mockWebScheduler(wrapUnique(new TestingPlatformMockScheduler)) { }
    ~TestingPlatformMockWebThread() override { }

    WebTaskRunner* getWebTaskRunner() override
    {
        return m_mockWebScheduler->timerTaskRunner();
    }

    bool isCurrentThread() const override
    {
        NOTREACHED();
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
    std::unique_ptr<TestingPlatformMockScheduler> m_mockWebScheduler;
};

// TestingPlatformSupportWithMockScheduler definition:

TestingPlatformSupportWithMockScheduler::TestingPlatformSupportWithMockScheduler()
    : m_mockWebThread(wrapUnique(new TestingPlatformMockWebThread())) { }

TestingPlatformSupportWithMockScheduler::TestingPlatformSupportWithMockScheduler(const Config& config)
    : TestingPlatformSupport(config)
    , m_mockWebThread(wrapUnique(new TestingPlatformMockWebThread())) { }

TestingPlatformSupportWithMockScheduler::~TestingPlatformSupportWithMockScheduler() { }

WebThread* TestingPlatformSupportWithMockScheduler::currentThread()
{
    return m_mockWebThread.get();
}

TestingPlatformMockScheduler* TestingPlatformSupportWithMockScheduler::mockWebScheduler()
{
    return m_mockWebThread->mockWebScheduler();
}

class ScopedUnittestsEnvironmentSetup::DummyPlatform final : public blink::Platform {
public:
    DummyPlatform() { }

    blink::WebThread* currentThread() override
    {
        static DummyThread dummyThread;
        return &dummyThread;
    };
};

ScopedUnittestsEnvironmentSetup::ScopedUnittestsEnvironmentSetup(int argc, char** argv)
{
    base::CommandLine::Init(argc, argv);

    base::test::InitializeICUForTesting();

    m_discardableMemoryAllocator = wrapUnique(new base::TestDiscardableMemoryAllocator);
    base::DiscardableMemoryAllocator::SetInstance(m_discardableMemoryAllocator.get());
    base::StatisticsRecorder::Initialize();

    m_platform = wrapUnique(new DummyPlatform);
    Platform::setCurrentPlatformForTesting(m_platform.get());

    WTF::Partitions::initialize(nullptr);
    WTF::setTimeFunctionsForTesting(dummyCurrentTime);
    WTF::initialize(nullptr);

    m_compositorSupport = wrapUnique(new cc_blink::WebCompositorSupportImpl);
    m_testingPlatformConfig.compositorSupport = m_compositorSupport.get();
    m_testingPlatformSupport = wrapUnique(new TestingPlatformSupport(m_testingPlatformConfig));

    ProcessHeap::init();
    ThreadState::attachMainThread();
    ThreadState::current()->registerTraceDOMWrappers(nullptr, nullptr, nullptr);
    EventTracer::initialize();
    HTTPNames::init();
}

ScopedUnittestsEnvironmentSetup::~ScopedUnittestsEnvironmentSetup()
{
}

} // namespace blink
