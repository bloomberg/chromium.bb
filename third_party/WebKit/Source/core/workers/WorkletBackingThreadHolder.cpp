// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletBackingThreadHolder.h"

#include "core/workers/WorkerBackingThread.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"

namespace blink {

WorkletBackingThreadHolder::WorkletBackingThreadHolder(
    std::unique_ptr<WorkerBackingThread> backingThread)
    : m_thread(std::move(backingThread)), m_initialized(false) {
  DCHECK(isMainThread());
  m_thread->backingThread().postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&WorkletBackingThreadHolder::initializeOnThread,
                      crossThreadUnretained(this)));
}

void WorkletBackingThreadHolder::shutdownAndWait() {
  DCHECK(isMainThread());
  WaitableEvent waitableEvent;
  m_thread->backingThread().postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&WorkletBackingThreadHolder::shutdownOnThread,
                      crossThreadUnretained(this),
                      crossThreadUnretained(&waitableEvent)));
  waitableEvent.wait();
}

void WorkletBackingThreadHolder::shutdownOnThread(
    WaitableEvent* waitableEvent) {
  m_thread->shutdown();
  waitableEvent->signal();
}

}  // namespace blink
