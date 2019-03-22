// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/common/training_example.h"

namespace media {
namespace learning {

// Controller for a single learning task.  Takes training examples, and forwards
// them to the learner(s).  Responsible for things like:
//  - Managing underlying learner(s) based on the learning task
//  - Feature subset selection
//  - UMA reporting on accuracy / feature importance
//
// The idea is that one can create a LearningTask, give it to an LTC, and the
// LTC will do the work of building / evaluating the model based on training
// examples that are provided to it.
class COMPONENT_EXPORT(LEARNING_IMPL) LearningTaskController {
 public:
  LearningTaskController() = default;
  virtual ~LearningTaskController() = default;

  // Receive an example for this task.
  virtual void AddExample(const TrainingExample& example) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LearningTaskController);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_H_
