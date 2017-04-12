// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerBackingThread.h"

#include <memory>
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IdleTaskRunner.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

#define DEFINE_STATIC_LOCAL_WITH_LOCK(type, name, arguments) \
  ASSERT(IsolatesMutex().Locked());                          \
  static type& name = *new type arguments

static Mutex& IsolatesMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, new Mutex);
  return mutex;
}

static HashSet<v8::Isolate*>& Isolates() {
  DEFINE_STATIC_LOCAL_WITH_LOCK(HashSet<v8::Isolate*>, isolates, ());
  return isolates;
}

static void AddWorkerIsolate(v8::Isolate* isolate) {
  MutexLocker lock(IsolatesMutex());
  Isolates().insert(isolate);
}

static void RemoveWorkerIsolate(v8::Isolate* isolate) {
  MutexLocker lock(IsolatesMutex());
  Isolates().erase(isolate);
}

WorkerBackingThread::WorkerBackingThread(const char* name,
                                         bool should_call_gc_on_shutdown)
    : backing_thread_(WebThreadSupportingGC::Create(name)),
      is_owning_thread_(true),
      should_call_gc_on_shutdown_(should_call_gc_on_shutdown) {}

WorkerBackingThread::WorkerBackingThread(WebThread* thread,
                                         bool should_call_gc_on_shutdown)
    : backing_thread_(WebThreadSupportingGC::CreateForThread(thread)),
      is_owning_thread_(false),
      should_call_gc_on_shutdown_(should_call_gc_on_shutdown) {}

WorkerBackingThread::~WorkerBackingThread() {}

void WorkerBackingThread::Initialize() {
  DCHECK(!isolate_);
  backing_thread_->Initialize();
  isolate_ = V8PerIsolateData::Initialize(
      backing_thread_->PlatformThread().GetWebTaskRunner());
  AddWorkerIsolate(isolate_);
  V8Initializer::InitializeWorker(isolate_);

  ThreadState::Current()->RegisterTraceDOMWrappers(
      isolate_, V8GCController::TraceDOMWrappers,
      ScriptWrappableVisitor::InvalidateDeadObjectsInMarkingDeque,
      ScriptWrappableVisitor::PerformCleanup);
  if (RuntimeEnabledFeatures::v8IdleTasksEnabled())
    V8PerIsolateData::EnableIdleTasks(
        isolate_, WTF::WrapUnique(new V8IdleTaskRunner(
                      BackingThread().PlatformThread().Scheduler())));
  if (is_owning_thread_)
    Platform::Current()->DidStartWorkerThread();

  V8PerIsolateData::From(isolate_)->SetThreadDebugger(
      WTF::MakeUnique<WorkerThreadDebugger>(isolate_));
}

void WorkerBackingThread::Shutdown() {
  if (is_owning_thread_)
    Platform::Current()->WillStopWorkerThread();

  V8PerIsolateData::WillBeDestroyed(isolate_);
  // TODO(yhirano): Remove this when https://crbug.com/v8/1428 is fixed.
  if (should_call_gc_on_shutdown_) {
    // This statement runs only in tests.
    V8GCController::CollectAllGarbageForTesting(isolate_);
  }
  backing_thread_->Shutdown();

  RemoveWorkerIsolate(isolate_);
  V8PerIsolateData::Destroy(isolate_);
  isolate_ = nullptr;
}

// static
void WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level) {
  MutexLocker lock(IsolatesMutex());
  for (v8::Isolate* isolate : Isolates())
    isolate->MemoryPressureNotification(level);
}

// static
void WorkerBackingThread::SetRAILModeOnWorkerThreadIsolates(
    v8::RAILMode rail_mode) {
  MutexLocker lock(IsolatesMutex());
  for (v8::Isolate* isolate : Isolates())
    isolate->SetRAILMode(rail_mode);
}

}  // namespace blink
