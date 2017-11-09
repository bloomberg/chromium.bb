// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/WebThreadSupportingGC.h"

#include <memory>
#include "platform/MemoryCoordinator.h"
#include "platform/heap/SafePoint.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Threading.h"

namespace blink {

std::unique_ptr<WebThreadSupportingGC> WebThreadSupportingGC::Create(
    const char* name) {
  return WTF::WrapUnique(new WebThreadSupportingGC(name, nullptr));
}

std::unique_ptr<WebThreadSupportingGC> WebThreadSupportingGC::CreateForThread(
    WebThread* thread) {
  return WTF::WrapUnique(new WebThreadSupportingGC(nullptr, thread));
}

WebThreadSupportingGC::WebThreadSupportingGC(const char* name,
                                             WebThread* thread)
    : thread_(thread) {
  DCHECK(IsMainThread());
  DCHECK(!name || !thread);
#if DCHECK_IS_ON()
  // We call this regardless of whether an existing thread is given or not,
  // as it means that blink is going to run with more than one thread.
  WTF::WillCreateThread();
#endif
  if (!thread_) {
    // If |thread| is not given, create a new one and own it.
    owning_thread_ = Platform::Current()->CreateThread(name);
    thread_ = owning_thread_.get();
  }
  MemoryCoordinator::RegisterThread(thread_);
}

WebThreadSupportingGC::~WebThreadSupportingGC() {
  DCHECK(IsMainThread());
  // WebThread's destructor blocks until all the tasks are processed.
  owning_thread_.reset();
  MemoryCoordinator::UnregisterThread(thread_);
}

void WebThreadSupportingGC::InitializeOnThread() {
  DCHECK(thread_->IsCurrentThread());
  ThreadState::AttachCurrentThread();
  gc_task_runner_ = std::make_unique<GCTaskRunner>(thread_);
}

void WebThreadSupportingGC::ShutdownOnThread() {
  DCHECK(thread_->IsCurrentThread());
#if defined(LEAK_SANITIZER)
  ThreadState::Current()->ReleaseStaticPersistentNodes();
#endif
  // Ensure no posted tasks will run from this point on.
  gc_task_runner_.reset();

  // Shutdown the thread (via its scheduler) only when the thread is created
  // and is owned by this instance.
  if (owning_thread_)
    owning_thread_->Scheduler()->Shutdown();

  ThreadState::DetachCurrentThread();
}

}  // namespace blink
