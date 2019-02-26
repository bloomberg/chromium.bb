// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "media/learning/impl/learning_task_controller.h"

namespace media {
namespace learning {

class COMPONENT_EXPORT(LEARNING_IMPL) LearningTaskControllerImpl
    : public LearningTaskController {
 public:
  explicit LearningTaskControllerImpl(const LearningTask& task);
  ~LearningTaskControllerImpl() override;

  // LearningTaskController
  void AddExample(const TrainingExample& example) override;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
