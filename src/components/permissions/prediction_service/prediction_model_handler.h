// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_H_

#include "base/run_loop.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/permissions/prediction_service/prediction_model_executor.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {
class PredictionModelHandler : public KeyedService,
                               public optimization_guide::ModelHandler<
                                   GeneratePredictionsResponse,
                                   const GeneratePredictionsRequest&> {
 public:
  explicit PredictionModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  ~PredictionModelHandler() override = default;
  PredictionModelHandler(const PredictionModelHandler&) = delete;
  PredictionModelHandler& operator=(const PredictionModelHandler&) = delete;

  // optimization_guide::ModelHandler overrides.
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

  void WaitForModelLoadForTesting();

 private:
  base::RunLoop model_load_run_loop_;
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_H_
