// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTaskRunner.h"

namespace blink {

// This class holds a reference to a TaskHandle to keep it alive while a task is
// pending in a task queue, and clears the reference on the task disposal, so
// that it doesn't leave a circular reference like below:
//   struct Foo : GarbageCollected<Foo> {
//     void bar() {}
//     RefPtr<TaskHandle> m_handle;
//   };
//
//   foo->m_handle = taskRunner->postCancellableTask(
//       BLINK_FROM_HERE, WTF::bind(&Foo::bar, wrapPersistent(foo)));
//
// There is a circular reference in the example above as:
//   foo -> m_handle -> m_task -> Persistent<Foo> in WTF::bind.
// CancelOnTaskDestruction is needed to break the circle by clearing |m_task|
// when the wrapped WTF::Closure is deleted.
class TaskHandle::CancelOnTaskDestruction {
 public:
  explicit CancelOnTaskDestruction(RefPtr<TaskHandle> handle)
      : m_handle(std::move(handle)) {}

  CancelOnTaskDestruction(CancelOnTaskDestruction&&) = default;

  void cancel() const {
    if (m_handle)
      m_handle->cancel();
  }

  ~CancelOnTaskDestruction() { cancel(); }

 private:
  RefPtr<TaskHandle> m_handle;
  DISALLOW_COPY_AND_ASSIGN(CancelOnTaskDestruction);
};

bool TaskHandle::isActive() const {
  return static_cast<bool>(m_task);
}

void TaskHandle::cancel() {
  std::unique_ptr<WTF::Closure> task = std::move(m_task);
  m_weakPtrFactory.revokeAll();
}

TaskHandle::~TaskHandle() {}

TaskHandle::TaskHandle(std::unique_ptr<WTF::Closure> task)
    : m_task(std::move(task)), m_weakPtrFactory(this) {
  DCHECK(m_task);
}

void TaskHandle::run(const CancelOnTaskDestruction& scoper) {
  std::unique_ptr<WTF::Closure> task = std::move(m_task);
  cancel();
  (*task)();
}

WTF::WeakPtr<TaskHandle> TaskHandle::asWeakPtr() {
  return m_weakPtrFactory.createWeakPtr();
}

void WebTaskRunner::postTask(const WebTraceLocation& location,
                             std::unique_ptr<CrossThreadClosure> task) {
  toSingleThreadTaskRunner()->PostTask(location,
                                       convertToBaseCallback(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<CrossThreadClosure> task,
                                    long long delayMs) {
  toSingleThreadTaskRunner()->PostDelayedTask(
      location, convertToBaseCallback(std::move(task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

void WebTaskRunner::postTask(const WebTraceLocation& location,
                             std::unique_ptr<WTF::Closure> task) {
  toSingleThreadTaskRunner()->PostTask(location,
                                       convertToBaseCallback(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<WTF::Closure> task,
                                    long long delayMs) {
  toSingleThreadTaskRunner()->PostDelayedTask(
      location, convertToBaseCallback(std::move(task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

RefPtr<TaskHandle> WebTaskRunner::postCancellableTask(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task) {
  DCHECK(runsTasksOnCurrentThread());
  RefPtr<TaskHandle> handle = adoptRef(new TaskHandle(std::move(task)));
  postTask(location, WTF::bind(&TaskHandle::run, handle->asWeakPtr(),
                               TaskHandle::CancelOnTaskDestruction(handle)));
  return handle;
}

RefPtr<TaskHandle> WebTaskRunner::postDelayedCancellableTask(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task,
    long long delayMs) {
  DCHECK(runsTasksOnCurrentThread());
  RefPtr<TaskHandle> handle = adoptRef(new TaskHandle(std::move(task)));
  postDelayedTask(location,
                  WTF::bind(&TaskHandle::run, handle->asWeakPtr(),
                            TaskHandle::CancelOnTaskDestruction(handle)),
                  delayMs);
  return handle;
}

}  // namespace blink
