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

#ifndef TestingPlatformSupport_h
#define TestingPlatformSupport_h

#include "platform/PlatformExport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebDiscardableMemory.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/Vector.h"

namespace blink {

class TestingPlatformMockWebTaskRunner;
class TestingPlatformMockWebThread;
class WebCompositorSupport;
class WebThread;

class TestingDiscardableMemory : public WebDiscardableMemory {
public:
    explicit TestingDiscardableMemory(size_t);
    ~TestingDiscardableMemory() override;

    // WebDiscardableMemory:
    bool lock() override;
    void* data() override;
    void unlock() override;
    WebMemoryAllocatorDump* createMemoryAllocatorDump(const WebString& name, WebProcessMemoryDump*) const override;

private:
    Vector<char> m_data;
    bool m_isLocked;
};

class TestingCompositorSupport : public WebCompositorSupport {
};

class TestingPlatformMockScheduler : public WebScheduler {
    WTF_MAKE_NONCOPYABLE(TestingPlatformMockScheduler);
public:
    TestingPlatformMockScheduler();
    ~TestingPlatformMockScheduler() override;

    void runSingleTask();
    void runAllTasks();

    // WebScheduler implementation:
    WebTaskRunner* loadingTaskRunner() override;
    WebTaskRunner* timerTaskRunner() override;
    void shutdown() override {}
    bool shouldYieldForHighPriorityWork() override { return false; }
    bool canExceedIdleDeadlineIfRequired() override { return false; }
    void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override { }
    void postNonNestableIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override { }
    void postIdleTaskAfterWakeup(const WebTraceLocation&, WebThread::IdleTask*) override { }
    WebPassOwnPtr<WebViewScheduler> createWebViewScheduler(blink::WebView*) override { return nullptr; }
    void suspendTimerQueue() override { }
    void resumeTimerQueue() override { }
    void addPendingNavigation() override { }
    void removePendingNavigation() override { }
    void onNavigationStarted() override { }

private:
    WTF::Deque<OwnPtr<WebTaskRunner::Task>> m_tasks;
    OwnPtr<TestingPlatformMockWebTaskRunner> m_mockWebTaskRunner;
};

class TestingPlatformSupport : public Platform {
    WTF_MAKE_NONCOPYABLE(TestingPlatformSupport);
public:
    struct Config {
        Config()
            : hasDiscardableMemorySupport(false)
            , compositorSupport(nullptr) { }

        bool hasDiscardableMemorySupport;
        WebCompositorSupport* compositorSupport;
    };

    TestingPlatformSupport();
    explicit TestingPlatformSupport(const Config&);

    ~TestingPlatformSupport() override;

    // Platform:
    WebDiscardableMemory* allocateAndLockDiscardableMemory(size_t bytes) override;
    WebString defaultLocale() override;
    WebCompositorSupport* compositorSupport() override;
    WebThread* currentThread() override;
    WebUnitTestSupport* unitTestSupport() override;

protected:
    const Config m_config;
    Platform* const m_oldPlatform;
};

class TestingPlatformSupportWithMockScheduler : public TestingPlatformSupport {
    WTF_MAKE_NONCOPYABLE(TestingPlatformSupportWithMockScheduler);
public:
    TestingPlatformSupportWithMockScheduler();
    explicit TestingPlatformSupportWithMockScheduler(const Config&);
    ~TestingPlatformSupportWithMockScheduler() override;

    // Platform:
    WebThread* currentThread() override;
    TestingPlatformMockScheduler* mockWebScheduler();

protected:
    OwnPtr<TestingPlatformMockWebThread> m_mockWebThread;
};

} // namespace blink

#endif // TestingPlatformSupport_h
