// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"

#include "base/bind.h"

namespace blink {

namespace {

void RunCrossThreadClosure(WTF::CrossThreadClosure task) {
  std::move(task).Run();
}

}  // namespace

// In those functions, we must use plain base::BindOnce() because:
//
//   * WTF::Bind() does thread checks which isn't compatible with our use case.
//   * CrossThreadBind() returns WTF::CrossThreadFunction which isn't
//     convertible to base::OnceClosure (this is actually a chicken-and-egg;
//     we need base::BindOnce() as an escape hatch).
void PostCrossThreadTask(base::SequencedTaskRunner& task_runner,
                         const base::Location& location,
                         WTF::CrossThreadClosure task) {
  task_runner.PostDelayedTask(
      location, base::BindOnce(&RunCrossThreadClosure, std::move(task)),
      base::TimeDelta());
}

void PostDelayedCrossThreadTask(base::SequencedTaskRunner& task_runner,
                                const base::Location& location,
                                WTF::CrossThreadClosure task,
                                base::TimeDelta delay) {
  task_runner.PostDelayedTask(
      location, base::BindOnce(&RunCrossThreadClosure, std::move(task)), delay);
}

}  // namespace blink
