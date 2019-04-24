// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_DISTRIBUTION_REPORTER_H_
#define MEDIA_LEARNING_IMPL_DISTRIBUTION_REPORTER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/model.h"
#include "media/learning/impl/target_distribution.h"

namespace media {
namespace learning {

// Helper class to report on predicted distrubutions vs target distributions.
// Use DistributionReporter::Create() to create one that's appropriate for a
// specific learning task.
class COMPONENT_EXPORT(LEARNING_IMPL) DistributionReporter {
 public:
  // Create a DistributionReporter that's suitable for |task|.
  static std::unique_ptr<DistributionReporter> Create(const LearningTask& task);

  virtual ~DistributionReporter();

  // Returns a prediction CB that will be compared to |observed|.  |observed| is
  // the total number of counts that we observed.
  virtual Model::PredictionCB GetPredictionCallback(
      TargetDistribution observed);

 protected:
  DistributionReporter(const LearningTask& task);

  const LearningTask& task() const { return task_; }

  // Implemented by subclasses to report a prediction.
  virtual void OnPrediction(TargetDistribution observed,
                            TargetDistribution predicted) = 0;

 private:
  LearningTask task_;

  base::WeakPtrFactory<DistributionReporter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DistributionReporter);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_DISTRIBUTION_REPORTER_H_
