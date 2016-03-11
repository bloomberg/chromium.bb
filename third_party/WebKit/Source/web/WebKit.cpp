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
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "gin/public/v8_platform.h"
#include "modules/InitModules.h"
#include "platform/Histogram.h"
#include "platform/LayoutTestSupport.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ThreadSafeFunctional.h"
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

} // namespace

static WebThread::TaskObserver* s_endOfTaskRunner = nullptr;
static GCTaskRunner* s_gcTaskRunner = nullptr;

// Make sure we are not re-initialized in the same address space.
// Doing so may cause hard to reproduce crashes.
static bool s_webKitInitialized = false;

static ModulesInitializer& modulesInitializer()
{
    DEFINE_STATIC_LOCAL(OwnPtr<ModulesInitializer>, initializer, (adoptPtr(new ModulesInitializer)));
    return *initializer;
}

void initialize(Platform* platform)
{
    initializeWithoutV8(platform);

    modulesInitializer().init();
    setIndexedDBClientCreateFunction(IndexedDBClientImpl::create);

    V8Initializer::initializeMainThread();

    // currentThread is null if we are running on a thread without a message loop.
    if (WebThread* currentThread = platform->currentThread()) {
        ASSERT(!s_endOfTaskRunner);
        s_endOfTaskRunner = new EndOfTaskRunner;
        currentThread->addTaskObserver(s_endOfTaskRunner);
    }
}

v8::Isolate* mainThreadIsolate()
{
    return V8PerIsolateData::mainThreadIsolate();
}

static void maxObservedSizeFunction(size_t sizeInMB)
{
    const size_t supportedMaxSizeInMB = 4 * 1024;
    if (sizeInMB >= supportedMaxSizeInMB)
        sizeInMB = supportedMaxSizeInMB - 1;

    // Send a UseCounter only when we see the highest memory usage
    // we've ever seen.
    DEFINE_STATIC_LOCAL(EnumerationHistogram, committedSizeHistogram, ("PartitionAlloc.CommittedSize", supportedMaxSizeInMB));
    committedSizeHistogram.count(sizeInMB);
}

static void callOnMainThreadFunction(WTF::MainThreadFunction function, void* context)
{
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(function, AllowCrossThreadAccess(context)));
}

void initializeWithoutV8(Platform* platform)
{
    ASSERT(!s_webKitInitialized);
    s_webKitInitialized = true;

    WTF::Partitions::initialize(maxObservedSizeFunction);
    WTF::initialize();
    WTF::initializeMainThread(callOnMainThreadFunction);

    ASSERT(platform);
    Platform::initialize(platform);
    Heap::init();

    ThreadState::attachMainThread();
    // currentThread() is null if we are running on a thread without a message loop.
    if (WebThread* currentThread = platform->currentThread()) {
        ASSERT(!s_gcTaskRunner);
        s_gcTaskRunner = new GCTaskRunner(currentThread);
    }
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

    ThreadState::current()->cleanupMainThread();

    // currentThread() is null if we are running on a thread without a message loop.
    if (Platform::current()->currentThread()) {
        // We don't need to (cannot) remove s_endOfTaskRunner from the current
        // message loop, because the message loop is already destructed before
        // the shutdown() is called.
        delete s_endOfTaskRunner;
        s_endOfTaskRunner = nullptr;
    }

    V8Initializer::shutdownMainThread();

    modulesInitializer().shutdown();

    shutdownWithoutV8();
}

void shutdownWithoutV8()
{
    ASSERT(isMainThread());
    // currentThread() is null if we are running on a thread without a message loop.
    if (Platform::current()->currentThread()) {
        ASSERT(s_gcTaskRunner);
        delete s_gcTaskRunner;
        s_gcTaskRunner = nullptr;
    }

    // Detach the main thread before starting the shutdown sequence
    // so that the main thread won't get involved in a GC during the shutdown.
    ThreadState::detachMainThread();

    ASSERT(!s_endOfTaskRunner);
    Heap::shutdown();
    Platform::shutdown();

    WTF::shutdown();
    WebPrerenderingSupport::shutdown();
    WTF::Partitions::shutdown();
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
