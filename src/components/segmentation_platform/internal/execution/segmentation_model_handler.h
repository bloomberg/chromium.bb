// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SEGMENTATION_MODEL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SEGMENTATION_MODEL_HANDLER_H_

#include <memory>
#include <vector>

#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {
namespace proto {
class SegmentationModelMetadata;
}  // namespace proto

// A simple wrapper around a ModelHandler which is usable for all segmentation
// models. This class constructs and owns the SegmentationModelExecutor through
// its parent class.
// See documentation for SegmentationModelExecutor for details on the
// requirements for the ML model and the inputs to execution.
class SegmentationModelHandler
    : public optimization_guide::ModelHandler<float,
                                              const std::vector<float>&> {
 public:
  using ModelUpdatedCallback = base::RepeatingCallback<void(
      optimization_guide::proto::OptimizationTarget,
      proto::SegmentationModelMetadata)>;

  explicit SegmentationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      optimization_guide::proto::OptimizationTarget optimization_target,
      const ModelUpdatedCallback& model_updated_callback);
  ~SegmentationModelHandler() override;

  // Disallow copy/assign.
  SegmentationModelHandler(const SegmentationModelHandler&) = delete;
  SegmentationModelHandler& operator=(const SegmentationModelHandler&) = delete;

  // optimization_guide::ModelHandler overrides.
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

 private:
  // Callback to invoke whenever the model file has been updated. If there is
  // a model available, this will be invoked at least once per session.
  ModelUpdatedCallback model_updated_callback_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SEGMENTATION_MODEL_HANDLER_H_
