// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_delayed_task_controller.h"

#include <stddef.h>

#include "base/logging.h"

namespace gcm {

GCMDelayedTaskController::GCMDelayedTaskController() : ready_(false) {
}

GCMDelayedTaskController::~GCMDelayedTaskController() {
}

void GCMDelayedTaskController::AddTask(const base::Closure& task) {
  delayed_tasks_.push_back(task);
}

void GCMDelayedTaskController::SetReady() {
  ready_ = true;
  RunTasks();
}

bool GCMDelayedTaskController::CanRunTaskWithoutDelay() const {
  return ready_;
}

void GCMDelayedTaskController::RunTasks() {
  DCHECK(ready_);

  for (size_t i = 0; i < delayed_tasks_.size(); ++i)
    delayed_tasks_[i].Run();
  delayed_tasks_.clear();
}

}  // namespace gcm
