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

#include "controller/BlinkInitializer.h"

#include "bindings/core/v8/V8Initializer.h"
#include "bindings/modules/v8/V8ContextSnapshotExternalReferences.h"
#include "build/build_config.h"
#include "controller/OomInterventionImpl.h"
#include "core/animation/AnimationClock.h"
#include "core/frame/LocalFrame.h"
#include "platform/Histogram.h"
#include "platform/bindings/Microtask.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/WTF.h"
#include "public/platform/InterfaceRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/web/WebKit.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
 public:
  void WillProcessTask() override { AnimationClock::NotifyTaskStart(); }
  void DidProcessTask() override {
    Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());
    V8Initializer::ReportRejectedPromisesOnMainThread();
  }
};

}  // namespace

static WebThread::TaskObserver* g_end_of_task_runner = nullptr;

static BlinkInitializer& GetBlinkInitializer() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<BlinkInitializer>, initializer,
                      (WTF::WrapUnique(new BlinkInitializer)));
  return *initializer;
}

void Initialize(Platform* platform, InterfaceRegistry* registry) {
  DCHECK(registry);
  Platform::Initialize(platform);

#if !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) && defined(OS_WIN)
  // Reserve address space on 32 bit Windows, to make it likelier that large
  // array buffer allocations succeed.
  BOOL is_wow_64 = -1;
  if (!IsWow64Process(GetCurrentProcess(), &is_wow_64))
    is_wow_64 = FALSE;
  if (!is_wow_64) {
    // Try to reserve as much address space as we reasonably can.
    const size_t kMB = 1024 * 1024;
    for (size_t size = 512 * kMB; size >= 32 * kMB; size -= 16 * kMB) {
      if (base::ReserveAddressSpace(size)) {
        // Report successful reservation.
        DEFINE_STATIC_LOCAL(CustomCountHistogram, reservation_size_histogram,
                            ("Renderer4.ReservedMemory", 32, 512, 32));
        reservation_size_histogram.Count(size / kMB);

        break;
      }
    }
  }
#endif  // !defined(ARCH_CPU_X86_64) && !defined(ARCH_CPU_ARM64) &&
        // defined(OS_WIN)

  V8Initializer::InitializeMainThread(
      V8ContextSnapshotExternalReferences::GetTable());

  GetBlinkInitializer().Initialize();

  GetBlinkInitializer().RegisterInterfaces(*registry);

  // currentThread is null if we are running on a thread without a message loop.
  if (WebThread* current_thread = platform->CurrentThread()) {
    DCHECK(!g_end_of_task_runner);
    g_end_of_task_runner = new EndOfTaskRunner;
    current_thread->AddTaskObserver(g_end_of_task_runner);
  }
}

void BlinkInitializer::RegisterInterfaces(InterfaceRegistry& registry) {
  ModulesInitializer::RegisterInterfaces(registry);
  WebThread* main_thread = Platform::Current()->MainThread();
  // GetSingleThreadTaskRunner() uses GetWebTaskRunner() internally.
  // crbug.com/781664
  if (!main_thread || !main_thread->GetWebTaskRunner())
    return;

  registry.AddInterface(CrossThreadBind(&OomInterventionImpl::Create),
                        main_thread->GetSingleThreadTaskRunner());
}

}  // namespace blink
