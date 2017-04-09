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

#include "public/platform/Platform.h"

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "platform/Histogram.h"
#include "platform/MemoryCoordinator.h"
#include "platform/PartitionAllocMemoryDumpProvider.h"
#include "platform/fonts/FontCacheMemoryDumpProvider.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/GCTaskRunner.h"
#include "platform/instrumentation/tracing/MemoryCacheDumpProvider.h"
#include "platform/wtf/HashMap.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/WebPrerenderingSupport.h"
#include "services/service_manager/public/cpp/connector.h"

namespace blink {

namespace {

class DefaultConnector {
 public:
  DefaultConnector() {
    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
  }

  service_manager::Connector* Get() { return connector_.get(); }

 private:
  std::unique_ptr<service_manager::Connector> connector_;
};

}  // namespace

static Platform* g_platform = nullptr;

static GCTaskRunner* g_gc_task_runner = nullptr;

static void MaxObservedSizeFunction(size_t size_in_mb) {
  const size_t kSupportedMaxSizeInMB = 4 * 1024;
  if (size_in_mb >= kSupportedMaxSizeInMB)
    size_in_mb = kSupportedMaxSizeInMB - 1;

  // Send a UseCounter only when we see the highest memory usage
  // we've ever seen.
  DEFINE_STATIC_LOCAL(EnumerationHistogram, committed_size_histogram,
                      ("PartitionAlloc.CommittedSize", kSupportedMaxSizeInMB));
  committed_size_histogram.Count(size_in_mb);
}

static void CallOnMainThreadFunction(WTF::MainThreadFunction function,
                                     void* context) {
  Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(function, CrossThreadUnretained(context)));
}

Platform::Platform() : main_thread_(0) {
  WTF::Partitions::Initialize(MaxObservedSizeFunction);
}

void Platform::Initialize(Platform* platform) {
  ASSERT(!g_platform);
  ASSERT(platform);
  g_platform = platform;
  g_platform->main_thread_ = platform->CurrentThread();

  WTF::Initialize(CallOnMainThreadFunction);

  ProcessHeap::Init();
  MemoryCoordinator::Initialize();
  if (base::ThreadTaskRunnerHandle::IsSet())
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        BlinkGCMemoryDumpProvider::Instance(), "BlinkGC",
        base::ThreadTaskRunnerHandle::Get());

  ThreadState::AttachMainThread();

  // TODO(ssid): remove this check after fixing crbug.com/486782.
  if (g_platform->main_thread_) {
    ASSERT(!g_gc_task_runner);
    g_gc_task_runner = new GCTaskRunner(g_platform->main_thread_);
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        PartitionAllocMemoryDumpProvider::Instance(), "PartitionAlloc",
        base::ThreadTaskRunnerHandle::Get());
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        FontCacheMemoryDumpProvider::Instance(), "FontCaches",
        base::ThreadTaskRunnerHandle::Get());
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        MemoryCacheDumpProvider::Instance(), "MemoryCache",
        base::ThreadTaskRunnerHandle::Get());
  }
}

void Platform::SetCurrentPlatformForTesting(Platform* platform) {
  ASSERT(platform);
  g_platform = platform;
  g_platform->main_thread_ = platform->CurrentThread();
}

Platform* Platform::Current() {
  return g_platform;
}

WebThread* Platform::MainThread() const {
  return main_thread_;
}

service_manager::Connector* Platform::GetConnector() {
  DEFINE_STATIC_LOCAL(DefaultConnector, connector, ());
  return connector.Get();
}

InterfaceProvider* Platform::GetInterfaceProvider() {
  return InterfaceProvider::GetEmptyInterfaceProvider();
}

}  // namespace blink
