// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/WebTaskRunner.h"

#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"

namespace base {

using RunnerMethodType =
    void (blink::TaskHandle::Runner::*)(const blink::TaskHandle&);

template <>
struct CallbackCancellationTraits<
    RunnerMethodType,
    std::tuple<WTF::WeakPtr<blink::TaskHandle::Runner>, blink::TaskHandle>> {
  static constexpr bool is_cancellable = true;

  static bool IsCancelled(RunnerMethodType,
                          const WTF::WeakPtr<blink::TaskHandle::Runner>&,
                          const blink::TaskHandle& handle) {
    return !handle.IsActive();
  }
};

}  // namespace base

namespace blink {

namespace {

void RunCrossThreadClosure(std::unique_ptr<CrossThreadClosure> task) {
  (*task)();
}

}  // namespace

class TaskHandle::Runner : public WTF::ThreadSafeRefCounted<Runner> {
 public:
  explicit Runner(std::unique_ptr<WTF::Closure> task)
      : task_(std::move(task)), weak_ptr_factory_(this) {}

  WTF::WeakPtr<Runner> AsWeakPtr() { return weak_ptr_factory_.CreateWeakPtr(); }

  bool IsActive() const { return task_ && !task_->IsCancelled(); }

  void Cancel() {
    std::unique_ptr<WTF::Closure> task = std::move(task_);
    weak_ptr_factory_.RevokeAll();
  }

  ~Runner() { Cancel(); }

  // The TaskHandle parameter on run() holds a reference to the Runner to keep
  // it alive while a task is pending in a task queue, and clears the reference
  // on the task disposal, so that it doesn't leave a circular reference like
  // below:
  //   struct Foo : GarbageCollected<Foo> {
  //     void bar() {}
  //     TaskHandle m_handle;
  //   };
  //
  //   foo.m_handle = taskRunner->postCancellableTask(
  //       BLINK_FROM_HERE, WTF::bind(&Foo::bar, wrapPersistent(foo)));
  //
  // There is a circular reference in the example above as:
  //   foo -> m_handle -> m_runner -> m_task -> Persistent<Foo> in WTF::bind.
  // The TaskHandle parameter on run() is needed to break the circle by clearing
  // |m_task| when the wrapped WTF::Closure is deleted.
  void Run(const TaskHandle&) {
    std::unique_ptr<WTF::Closure> task = std::move(task_);
    weak_ptr_factory_.RevokeAll();
    (*task)();
  }

 private:
  std::unique_ptr<WTF::Closure> task_;
  WTF::WeakPtrFactory<Runner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Runner);
};

bool TaskHandle::IsActive() const {
  return runner_ && runner_->IsActive();
}

void TaskHandle::Cancel() {
  if (runner_) {
    runner_->Cancel();
    runner_ = nullptr;
  }
}

TaskHandle::TaskHandle() = default;

TaskHandle::~TaskHandle() {
  Cancel();
}

TaskHandle::TaskHandle(TaskHandle&&) = default;

TaskHandle& TaskHandle::operator=(TaskHandle&& other) {
  TaskHandle tmp(std::move(other));
  runner_.Swap(tmp.runner_);
  return *this;
}

TaskHandle::TaskHandle(RefPtr<Runner> runner) : runner_(std::move(runner)) {
  DCHECK(runner_);
}

// Use a custom function for base::Bind instead of convertToBaseCallback to
// avoid copying the closure later in the call chain. Copying the bound state
// can lead to data races with ref counted objects like StringImpl. See
// crbug.com/679915 for more details.
void WebTaskRunner::PostTask(const WebTraceLocation& location,
                             std::unique_ptr<CrossThreadClosure> task) {
  ToSingleThreadTaskRunner()->PostTask(
      location, base::Bind(&RunCrossThreadClosure, base::Passed(&task)));
}

void WebTaskRunner::PostDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<CrossThreadClosure> task,
                                    TimeDelta delay) {
  ToSingleThreadTaskRunner()->PostDelayedTask(
      location, base::Bind(&RunCrossThreadClosure, base::Passed(&task)), delay);
}

void WebTaskRunner::PostTask(const WebTraceLocation& location,
                             std::unique_ptr<WTF::Closure> task) {
  ToSingleThreadTaskRunner()->PostTask(location,
                                       ConvertToBaseCallback(std::move(task)));
}

void WebTaskRunner::PostDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<WTF::Closure> task,
                                    TimeDelta delay) {
  ToSingleThreadTaskRunner()->PostDelayedTask(
      location, ConvertToBaseCallback(std::move(task)), delay);
}

TaskHandle WebTaskRunner::PostCancellableTask(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task) {
  DCHECK(RunsTasksInCurrentSequence());
  RefPtr<TaskHandle::Runner> runner =
      AdoptRef(new TaskHandle::Runner(std::move(task)));
  PostTask(location, WTF::Bind(&TaskHandle::Runner::Run, runner->AsWeakPtr(),
                               TaskHandle(runner)));
  return TaskHandle(runner);
}

TaskHandle WebTaskRunner::PostDelayedCancellableTask(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task,
    TimeDelta delay) {
  DCHECK(RunsTasksInCurrentSequence());
  RefPtr<TaskHandle::Runner> runner =
      AdoptRef(new TaskHandle::Runner(std::move(task)));
  PostDelayedTask(location,
                  WTF::Bind(&TaskHandle::Runner::Run, runner->AsWeakPtr(),
                            TaskHandle(runner)),
                  delay);
  return TaskHandle(runner);
}

WebTaskRunner::~WebTaskRunner() = default;

}  // namespace blink
