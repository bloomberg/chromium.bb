// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_session_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/learning/impl/learning_task_controller_impl.h"

namespace media {
namespace learning {

LearningSessionImpl::LearningSessionImpl()
    : controller_factory_(
          base::BindRepeating([](const LearningTask& task)
                                  -> std::unique_ptr<LearningTaskController> {
            return std::make_unique<LearningTaskControllerImpl>(task);
          })) {}

LearningSessionImpl::~LearningSessionImpl() = default;

void LearningSessionImpl::SetTaskControllerFactoryCBForTesting(
    CreateTaskControllerCB cb) {
  controller_factory_ = std::move(cb);
}

void LearningSessionImpl::AddExample(const std::string& task_name,
                                     const TrainingExample& example) {
  auto iter = task_map_.find(task_name);
  if (iter != task_map_.end())
    iter->second->AddExample(example);
}

void LearningSessionImpl::RegisterTask(const LearningTask& task) {
  DCHECK(task_map_.count(task.name) == 0);
  task_map_.emplace(task.name, controller_factory_.Run(task));
}

}  // namespace learning
}  // namespace media
