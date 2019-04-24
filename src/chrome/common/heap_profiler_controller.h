// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_HEAP_PROFILER_CONTROLLER_H_
#define CHROME_COMMON_HEAP_PROFILER_CONTROLLER_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class TaskRunner;
}  // namespace base

// HeapProfilerController controls collection of sampled heap allocation
// snapshots for the current process.
class HeapProfilerController {
 public:
  HeapProfilerController();
  ~HeapProfilerController();

  // Starts periodic heap snapshot collection.
  void StartIfEnabled();

  void SetTaskRunnerForTest(scoped_refptr<base::TaskRunner> task_runner) {
    task_runner_ = std::move(task_runner);
  }

 private:
  void ScheduleNextSnapshot();
  void TakeSnapshot();
  void RetrieveAndSendSnapshot();

  bool started_ = false;
  scoped_refptr<base::TaskRunner> task_runner_;
  base::WeakPtrFactory<HeapProfilerController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HeapProfilerController);
};

#endif  // CHROME_COMMON_HEAP_PROFILER_CONTROLLER_H_
