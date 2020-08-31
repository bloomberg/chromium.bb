// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_PARAMETERS_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_PARAMETERS_H_

#include <string>

#include "base/macros.h"

namespace background_task {

// TaskParameters are passed to BackgroundTask whenever they are invoked. It
// contains the task ID and the extras that the caller has passed in with the
// TaskInfo.
struct TaskParameters {
  TaskParameters();
  ~TaskParameters();

  int task_id;
  std::string extras;
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_PARAMETERS_H_
