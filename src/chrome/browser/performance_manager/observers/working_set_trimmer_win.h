// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_WORKING_SET_TRIMMER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_WORKING_SET_TRIMMER_WIN_H_

#include "base/macros.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace performance_manager {

// Empties the working set of processes in which all frames are frozen.
//
// Objective #1: Track working set growth rate.
//   Swap trashing occurs when a lot of pages are accessed in a short period of
//   time. Swap trashing can be reduced by reducing the number of pages accessed
//   by processes in which all frames are frozen. To track efforts towards this
//   goal, we empty the working set of processes when all their frames become
//   frozen and record the size of their working set after x minutes.
//   TODO(fdoray): Record the working set size x minutes after emptying it.
//   https://crbug.com/885293
//
// Objective #2: Improve performance.
//   We hypothesize that emptying the working set of a process causes its pages
//   to be compressed and/or written to disk preemptively, which makes more
//   memory available quickly for foreground processes and improves global
//   browser performance.
class WorkingSetTrimmer : public GraphObserverDefaultImpl {
 public:
  WorkingSetTrimmer();
  ~WorkingSetTrimmer() override;

  // GraphObserver:
  bool ShouldObserve(const NodeBase* node) override;
  void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmer);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_WORKING_SET_TRIMMER_WIN_H_
