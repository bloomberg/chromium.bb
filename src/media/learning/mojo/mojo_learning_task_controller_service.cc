// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/mojo_learning_task_controller_service.h"

#include <utility>

#include "media/learning/common/learning_task_controller.h"

namespace media {
namespace learning {

// Somewhat arbitrary upper limit on the number of in-flight observations that
// we'll allow a client to have.
static const size_t kMaxInFlightObservations = 16;

MojoLearningTaskControllerService::MojoLearningTaskControllerService(
    const LearningTask& task,
    std::unique_ptr<::media::learning::LearningTaskController> impl)
    : task_(task), impl_(std::move(impl)) {}

MojoLearningTaskControllerService::~MojoLearningTaskControllerService() =
    default;

void MojoLearningTaskControllerService::BeginObservation(
    const base::UnguessableToken& id,
    const FeatureVector& features) {
  // Drop the observation if it doesn't match the feature description size.
  if (features.size() != task_.feature_descriptions.size())
    return;

  // Don't allow the client to send too many in-flight observations.
  if (in_flight_observations_.size() >= kMaxInFlightObservations)
    return;
  in_flight_observations_.insert(id);

  // Since we own |impl_|, we don't need to keep track of in-flight
  // observations.  We'll release |impl_| on destruction, which cancels them.
  impl_->BeginObservation(id, features);
}

void MojoLearningTaskControllerService::CompleteObservation(
    const base::UnguessableToken& id,
    const ObservationCompletion& completion) {
  auto iter = in_flight_observations_.find(id);
  if (iter == in_flight_observations_.end())
    return;
  in_flight_observations_.erase(iter);

  impl_->CompleteObservation(id, completion);
}

void MojoLearningTaskControllerService::CancelObservation(
    const base::UnguessableToken& id) {
  auto iter = in_flight_observations_.find(id);
  if (iter == in_flight_observations_.end())
    return;
  in_flight_observations_.erase(iter);

  impl_->CancelObservation(id);
}

}  // namespace learning
}  // namespace media
