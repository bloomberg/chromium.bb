// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TASK_H_
#define COMPONENTS_UPDATE_CLIENT_TASK_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace update_client {

// Defines an abstraction for a unit of work done by the update client.
// Each invocation of the update client API results in a task being created and
// run. In most cases, a task corresponds to a set of CRXs, which are updated
// together.
class Task : public base::RefCounted<Task> {
 public:
  Task() = default;
  virtual void Run() = 0;

  // Does a best effort attempt to make a task release its resources and stop
  // soon. It is possible that a running task may complete even if this
  // method is called.
  virtual void Cancel() = 0;

  // Returns the ids corresponding to the CRXs associated with this update task.
  virtual std::vector<std::string> GetIds() const = 0;

 protected:
  friend class base::RefCounted<Task>;
  virtual ~Task() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Task);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TASK_H_
