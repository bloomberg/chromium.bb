// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/mojo_learning_session_impl.h"

#include "media/learning/common/learning_session.h"

namespace media {
namespace learning {

MojoLearningSessionImpl::MojoLearningSessionImpl(
    std::unique_ptr<::media::learning::LearningSession> impl)
    : impl_(std::move(impl)) {}

MojoLearningSessionImpl::~MojoLearningSessionImpl() = default;

void MojoLearningSessionImpl::Bind(mojom::LearningSessionRequest request) {
  self_bindings_.AddBinding(this, std::move(request));
}

void MojoLearningSessionImpl::AddExample(mojom::LearningTaskType task_type,
                                         const TrainingExample& example) {
  // TODO(liberato): Convert |task_type| into a task name.
  std::string task_name("no_task");

  impl_->AddExample(task_name, example);
}

}  // namespace learning
}  // namespace media
