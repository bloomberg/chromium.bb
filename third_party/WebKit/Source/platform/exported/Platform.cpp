/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "platform/Histogram.h"
#include "platform/PartitionAllocMemoryDumpProvider.h"
#include "platform/fonts/FontCacheMemoryDumpProvider.h"
#include "platform/graphics/CompositorFactory.h"
#include "platform/heap/GCTaskRunner.h"
#include "platform/web_memory_dump_provider_adapter.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "public/platform/WebPrerenderingSupport.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

namespace blink {

static Platform* s_platform = nullptr;
using ProviderToAdapterMap = HashMap<WebMemoryDumpProvider*, OwnPtr<WebMemoryDumpProviderAdapter>>;

static GCTaskRunner* s_gcTaskRunner = nullptr;

namespace {

ProviderToAdapterMap& memoryDumpProviders()
{
    DEFINE_STATIC_LOCAL(ProviderToAdapterMap, providerToAdapterMap, ());
    return providerToAdapterMap;
}

} // namespace

Platform::Platform()
    : m_mainThread(0)
{
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

void Platform::initialize(Platform* platform)
{
    ASSERT(!s_platform);
    ASSERT(platform);
    s_platform = platform;
    s_platform->m_mainThread = platform->currentThread();

    WTF::Partitions::initialize(maxObservedSizeFunction);
    WTF::initialize(callOnMainThreadFunction);

    ThreadHeap::init();

    ThreadState::attachMainThread();

    // TODO(ssid): remove this check after fixing crbug.com/486782.
    if (s_platform->m_mainThread) {
        ASSERT(!s_gcTaskRunner);
        s_gcTaskRunner = new GCTaskRunner(s_platform->m_mainThread);
        s_platform->registerMemoryDumpProvider(PartitionAllocMemoryDumpProvider::instance(), "PartitionAlloc");
        s_platform->registerMemoryDumpProvider(FontCacheMemoryDumpProvider::instance(), "FontCaches");
    }

    CompositorFactory::initializeDefault();
}

void Platform::shutdown()
{
    ASSERT(isMainThread());
    CompositorFactory::shutdown();

    if (s_platform->m_mainThread) {
        s_platform->unregisterMemoryDumpProvider(FontCacheMemoryDumpProvider::instance());
        s_platform->unregisterMemoryDumpProvider(PartitionAllocMemoryDumpProvider::instance());
        ASSERT(s_gcTaskRunner);
        delete s_gcTaskRunner;
        s_gcTaskRunner = nullptr;
    }

    // Detach the main thread before starting the shutdown sequence
    // so that the main thread won't get involved in a GC during the shutdown.
    ThreadState::detachMainThread();

    ThreadHeap::shutdown();

    WTF::shutdown();
    WTF::Partitions::shutdown();

    s_platform->m_mainThread = nullptr;
    s_platform = nullptr;
}

void Platform::setCurrentPlatformForTesting(Platform* platform)
{
    ASSERT(platform);
    s_platform = platform;
    s_platform->m_mainThread = platform->currentThread();
}

Platform* Platform::current()
{
    return s_platform;
}

WebThread* Platform::mainThread() const
{
    return m_mainThread;
}

void Platform::registerMemoryDumpProvider(WebMemoryDumpProvider* provider, const char* name)
{
    // MemoryDumpProvider needs a message loop.
    if (!Platform::current()->currentThread())
        return;

    WebMemoryDumpProviderAdapter* adapter = new WebMemoryDumpProviderAdapter(provider);
    ProviderToAdapterMap::AddResult result = memoryDumpProviders().add(provider, adoptPtr(adapter));
    if (!result.isNewEntry)
        return;
    adapter->set_is_registered(true);
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(adapter, name, base::ThreadTaskRunnerHandle::Get());
}

void Platform::unregisterMemoryDumpProvider(WebMemoryDumpProvider* provider)
{
    // MemoryDumpProvider needs a message loop.
    if (!Platform::current()->currentThread())
        return;

    ProviderToAdapterMap::iterator it = memoryDumpProviders().find(provider);
    if (it == memoryDumpProviders().end())
        return;
    WebMemoryDumpProviderAdapter* adapter = it->value.get();
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(adapter);
    adapter->set_is_registered(false);
    memoryDumpProviders().remove(it);
}

ServiceRegistry* Platform::serviceRegistry()
{
    return ServiceRegistry::getEmptyServiceRegistry();
}

} // namespace blink
