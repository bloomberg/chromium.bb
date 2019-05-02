// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TASK_RUNNER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TASK_RUNNER_H_

#include <list>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/base/task_runner.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) ScopedPerfettoPostTaskBlocker {
 public:
  explicit ScopedPerfettoPostTaskBlocker(bool enable);
  ~ScopedPerfettoPostTaskBlocker();

 private:
  const bool enabled_;
};

// This wraps a base::TaskRunner implementation to be able
// to provide it to Perfetto.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoTaskRunner
    : public perfetto::base::TaskRunner {
 public:
  explicit PerfettoTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~PerfettoTaskRunner() override;

  // perfetto::base::TaskRunner implementation. Only called by
  // the Perfetto implementation itself.
  void PostTask(std::function<void()> task) override;
  void PostDelayedTask(std::function<void()> task, uint32_t delay_ms) override;
  // This in Chrome would more correctly be called "RunsTasksInCurrentSequence".
  // Perfetto calls this to determine wheather CommitData requests should be
  // flushed synchronously. RunsTasksInCurrentSequence is sufficient for that
  // use case.
  bool RunsTasksOnCurrentThread() const override;

  void SetTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);
  scoped_refptr<base::SequencedTaskRunner> GetOrCreateTaskRunner();

  // Not used in Chrome.
  void AddFileDescriptorWatch(int fd, std::function<void()>) override;
  void RemoveFileDescriptorWatch(int fd) override;


  // Tests will shut down all task runners in between runs, so we need
  // to re-create any static instances on each SetUp();
  void ResetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Sometimes we have to temporarily defer any posted tasks, like
  // when trace events are added when the taskqueue is locked. For this purpose
  // we keep a timer running when tracing is enabled, which will periodically
  // drain these posted tasks.
  void StartDeferredTasksDrainTimer();
  void StopDeferredTasksDrainTimer();

  static void BlockPostTaskForThread();
  static void UnblockPostTaskForThread();

 private:
  void OnDeferredTasksDrainTimer();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::Lock lock_;  // Protects deferred_tasks_;
  std::list<std::function<void()>> deferred_tasks_;
  base::RepeatingTimer deferred_tasks_timer_;

  DISALLOW_COPY_AND_ASSIGN(PerfettoTaskRunner);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TASK_RUNNER_H_
