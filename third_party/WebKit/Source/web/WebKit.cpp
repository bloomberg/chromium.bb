/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebKit.h"

#include "bindings/core/v8/ScriptStreamerThread.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/Init.h"
#include "core/animation/AnimationClock.h"
#include "core/dom/Microtask.h"
#include "core/fetch/WebCacheMemoryDumpProvider.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "gin/public/v8_platform.h"
#include "modules/InitModules.h"
#include "platform/LayoutTestSupport.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCacheMemoryDumpProvider.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/heap/GCTaskRunner.h"
#include "platform/heap/Heap.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPrerenderingSupport.h"
#include "public/platform/WebThread.h"
#include "web/IndexedDBClientImpl.h"
#include "wtf/Assertions.h"
#include "wtf/CryptographicallyRandomNumber.h"
#include "wtf/MainThread.h"
#include "wtf/Partitions.h"
#include "wtf/WTF.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextEncoding.h"
#include <v8.h>

namespace blink {

namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
public:
    void willProcessTask() override
    {
        AnimationClock::notifyTaskStart();
    }
    void didProcessTask() override
    {
        Microtask::performCheckpoint(mainThreadIsolate());
        V8GCController::reportDOMMemoryUsageToV8(mainThreadIsolate());
        V8Initializer::reportRejectedPromisesOnMainThread();
    }
};

class MainThreadTaskRunner: public WebTaskRunner::Task {
    WTF_MAKE_NONCOPYABLE(MainThreadTaskRunner);
public:
    MainThreadTaskRunner(WTF::MainThreadFunction* function, void* context)
        : m_function(function)
        , m_context(context) { }

    void run() override
    {
        m_function(m_context);
    }
private:
    WTF::MainThreadFunction* m_function;
    void* m_context;
};

} // namespace

static WebThread::TaskObserver* s_endOfTaskRunner = nullptr;
static GCTaskRunner* s_gcTaskRunner = nullptr;

// Make sure we are not re-initialized in the same address space.
// Doing so may cause hard to reproduce crashes.
static bool s_webKitInitialized = false;

void initialize(Platform* platform)
{
    initializeWithoutV8(platform);

    V8Initializer::initializeMainThreadIfNeeded();

    OwnPtr<V8IsolateInterruptor> interruptor = adoptPtr(new V8IsolateInterruptor(V8PerIsolateData::mainThreadIsolate()));
    ThreadState::current()->addInterruptor(interruptor.release());
    ThreadState::current()->registerTraceDOMWrappers(V8PerIsolateData::mainThreadIsolate(), V8GCController::traceDOMWrappers);

    // currentThread is null if we are running on a thread without a message loop.
    if (WebThread* currentThread = platform->currentThread()) {
        ASSERT(!s_endOfTaskRunner);
        s_endOfTaskRunner = new EndOfTaskRunner;
        currentThread->addTaskObserver(s_endOfTaskRunner);

        // Register web cache dump provider for tracing.
        platform->registerMemoryDumpProvider(WebCacheMemoryDumpProvider::instance(), "MemoryCache");
        platform->registerMemoryDumpProvider(FontCacheMemoryDumpProvider::instance(), "FontCaches");
    }
}

v8::Isolate* mainThreadIsolate()
{
    return V8PerIsolateData::mainThreadIsolate();
}

static double currentTimeFunction()
{
    return Platform::current()->currentTimeSeconds();
}

static double monotonicallyIncreasingTimeFunction()
{
    return Platform::current()->monotonicallyIncreasingTimeSeconds();
}

static void histogramEnumerationFunction(const char* name, int sample, int boundaryValue)
{
    Platform::current()->histogramEnumeration(name, sample, boundaryValue);
}

static void callOnMainThreadFunction(WTF::MainThreadFunction function, void* context)
{
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, new MainThreadTaskRunner(function, context));
}

static void adjustAmountOfExternalAllocatedMemory(int size)
{
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(size);
}

void initializeWithoutV8(Platform* platform)
{
    ASSERT(!s_webKitInitialized);
    s_webKitInitialized = true;

    ASSERT(platform);
    Platform::initialize(platform);

    WTF::initialize(currentTimeFunction, monotonicallyIncreasingTimeFunction, histogramEnumerationFunction, adjustAmountOfExternalAllocatedMemory);
    WTF::initializeMainThread(callOnMainThreadFunction);
    Heap::init();

    ThreadState::attachMainThread();
    // currentThread() is null if we are running on a thread without a message loop.
    if (WebThread* currentThread = platform->currentThread()) {
        ASSERT(!s_gcTaskRunner);
        s_gcTaskRunner = new GCTaskRunner(currentThread);
    }

    DEFINE_STATIC_LOCAL(ModulesInitializer, initializer, ());
    initializer.init();

    setIndexedDBClientCreateFunction(IndexedDBClientImpl::create);
}

void shutdown()
{
#if defined(LEAK_SANITIZER)
    // If LSan is about to perform leak detection, release all the registered
    // static Persistent<> root references to global caches that Blink keeps,
    // followed by GCs to clear out all they referred to. A full v8 GC cycle
    // is needed to flush out all garbage.
    //
    // This is not needed for caches over non-Oilpan objects, as they're
    // not scanned by LSan due to being held in non-global storage
    // ("static" references inside functions/methods.)
    if (ThreadState* threadState = ThreadState::current()) {
        threadState->releaseStaticPersistentNodes();
        Heap::collectAllGarbage();
    }
#endif

    // currentThread() is null if we are running on a thread without a message loop.
    if (Platform::current()->currentThread()) {
        Platform::current()->unregisterMemoryDumpProvider(WebCacheMemoryDumpProvider::instance());
        Platform::current()->unregisterMemoryDumpProvider(FontCacheMemoryDumpProvider::instance());

        // We don't need to (cannot) remove s_endOfTaskRunner from the current
        // message loop, because the message loop is already destructed before
        // the shutdown() is called.
        delete s_endOfTaskRunner;
        s_endOfTaskRunner = nullptr;

        ASSERT(s_gcTaskRunner);
        delete s_gcTaskRunner;
        s_gcTaskRunner = nullptr;
    }

    // Shutdown V8-related background threads before V8 is ramped down. Note
    // that this will wait the thread to stop its operations.
    ScriptStreamerThread::shutdown();

    v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
    V8PerIsolateData::willBeDestroyed(isolate);

    // Make sure we stop WorkerThreads before the main thread's ThreadState
    // and later shutdown steps starts freeing up resources needed during
    // worker termination.
    WorkerThread::terminateAndWaitForAllWorkers();

    ModulesInitializer::terminateThreads();

    // Detach the main thread before starting the shutdown sequence
    // so that the main thread won't get involved in a GC during the shutdown.
    ThreadState::detachMainThread();

    V8PerIsolateData::destroy(isolate);

    shutdownWithoutV8();
}

void shutdownWithoutV8()
{
    ASSERT(!s_endOfTaskRunner);
    CoreInitializer::shutdown();
    Heap::shutdown();
    WTF::shutdown();
    Platform::shutdown();
    WebPrerenderingSupport::shutdown();
}

// TODO(tkent): The following functions to wrap LayoutTestSupport should be
// moved to public/platform/.

void setLayoutTestMode(bool value)
{
    LayoutTestSupport::setIsRunningLayoutTest(value);
}

bool layoutTestMode()
{
    return LayoutTestSupport::isRunningLayoutTest();
}

void setMockThemeEnabledForTest(bool value)
{
    LayoutTestSupport::setMockThemeEnabledForTest(value);
}

void setFontAntialiasingEnabledForTest(bool value)
{
    LayoutTestSupport::setFontAntialiasingEnabledForTest(value);
}

bool fontAntialiasingEnabledForTest()
{
    return LayoutTestSupport::isFontAntialiasingEnabledForTest();
}

void setAlwaysUseComplexTextForTest(bool value)
{
    LayoutTestSupport::setAlwaysUseComplexTextForTest(value);
}

bool alwaysUseComplexTextForTest()
{
    return LayoutTestSupport::alwaysUseComplexTextForTest();
}

void enableLogChannel(const char* name)
{
#if !LOG_DISABLED
    WTFLogChannel* channel = getChannelFromName(name);
    if (channel)
        channel->state = WTFLogChannelOn;
#endif // !LOG_DISABLED
}

void resetPluginCache(bool reloadPages)
{
    ASSERT(!reloadPages);
    Page::refreshPlugins();
}

void decommitFreeableMemory()
{
    WTF::Partitions::decommitFreeableMemory();
}

} // namespace blink
