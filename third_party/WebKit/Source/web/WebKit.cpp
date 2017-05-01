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

#include <memory>

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/animation/AnimationClock.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/Page.h"
#include "core/workers/WorkerBackingThread.h"
#include "gin/public/v8_platform.h"
#include "modules/ModulesInitializer.h"
#include "platform/LayoutTestSupport.h"
#include "platform/bindings/Microtask.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/WTF.h"
#include "platform/wtf/allocator/Partitions.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/TextEncoding.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
 public:
  void WillProcessTask() override { AnimationClock::NotifyTaskStart(); }
  void DidProcessTask() override {
    Microtask::PerformCheckpoint(MainThreadIsolate());
    V8Initializer::ReportRejectedPromisesOnMainThread();
  }
};

}  // namespace

static WebThread::TaskObserver* g_end_of_task_runner = nullptr;

static ModulesInitializer& GetModulesInitializer() {
  DEFINE_STATIC_LOCAL(std::unique_ptr<ModulesInitializer>, initializer,
                      (WTF::WrapUnique(new ModulesInitializer)));
  return *initializer;
}

void Initialize(Platform* platform) {
  Platform::Initialize(platform);

  V8Initializer::InitializeMainThread();

  GetModulesInitializer().Initialize();

  // currentThread is null if we are running on a thread without a message loop.
  if (WebThread* current_thread = platform->CurrentThread()) {
    DCHECK(!g_end_of_task_runner);
    g_end_of_task_runner = new EndOfTaskRunner;
    current_thread->AddTaskObserver(g_end_of_task_runner);
  }
}

v8::Isolate* MainThreadIsolate() {
  return V8PerIsolateData::MainThreadIsolate();
}

// TODO(tkent): The following functions to wrap LayoutTestSupport should be
// moved to public/platform/.

void SetLayoutTestMode(bool value) {
  LayoutTestSupport::SetIsRunningLayoutTest(value);
}

bool LayoutTestMode() {
  return LayoutTestSupport::IsRunningLayoutTest();
}

void SetMockThemeEnabledForTest(bool value) {
  LayoutTestSupport::SetMockThemeEnabledForTest(value);
  LayoutTheme::GetTheme().DidChangeThemeEngine();
}

void SetFontAntialiasingEnabledForTest(bool value) {
  LayoutTestSupport::SetFontAntialiasingEnabledForTest(value);
}

bool FontAntialiasingEnabledForTest() {
  return LayoutTestSupport::IsFontAntialiasingEnabledForTest();
}

void ResetPluginCache(bool reload_pages) {
  DCHECK(!reload_pages);
  Page::RefreshPlugins();
}

void DecommitFreeableMemory() {
  WTF::Partitions::DecommitFreeableMemory();
}

void MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level) {
  WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(level);
}

void SetRAILModeOnWorkerThreadIsolates(v8::RAILMode rail_mode) {
  WorkerBackingThread::SetRAILModeOnWorkerThreadIsolates(rail_mode);
}

}  // namespace blink
