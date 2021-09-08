// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_ABORTABLE_TASK_LIST_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_ABORTABLE_TASK_LIST_H_

#include <memory>
#include <vector>

namespace chromeos {
namespace libassistant {

class AbortableTask {
 public:
  AbortableTask() = default;
  virtual ~AbortableTask() = default;

  virtual bool IsFinished() = 0;
  virtual void Abort() = 0;
};

// Container used when we have to wait for a task to finish.
// Finished tasks will be removed from the list.
// All unfinished tasks will be aborted when the task list is destroyed,
// or when |AbortAll()| is called.
class AbortableTaskList {
 public:
  AbortableTaskList();
  AbortableTaskList(const AbortableTaskList&) = delete;
  AbortableTaskList& operator=(const AbortableTaskList&) = delete;
  ~AbortableTaskList();

  // Add the given task, and return a pointer. This method is templated so we
  // can return a pointer of the actual type (and not |AbortableTask*|).
  template <class ActualType>
  ActualType* Add(std::unique_ptr<ActualType> task) {
    ActualType* pointer = task.get();
    AddInternal(std::move(task));
    return pointer;
  }

  void AbortAll();

 private:
  void AddInternal(std::unique_ptr<AbortableTask> task);
  void RemoveFinishedTasks();

  std::vector<std::unique_ptr<AbortableTask>> tasks_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_ABORTABLE_TASK_LIST_H_
