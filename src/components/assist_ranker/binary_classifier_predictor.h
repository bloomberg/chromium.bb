// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_BINARY_CLASSIFIER_PREDICTOR_H_
#define COMPONENTS_ASSIST_RANKER_BINARY_CLASSIFIER_PREDICTOR_H_

#include "base/compiler_specific.h"
#include "components/assist_ranker/base_predictor.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace base {
class FilePath;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace assist_ranker {

class GenericLogisticRegressionInference;

// Predictor class for models that output a binary decision.
class BinaryClassifierPredictor : public BasePredictor {
 public:
  ~BinaryClassifierPredictor() override;

  // Returns an new predictor instance with the given |config| and initialize
  // its model loader. The |request_context getter| is passed to the
  // predictor's model_loader which holds it as scoped_refptr.
  static std::unique_ptr<BinaryClassifierPredictor> Create(
      const PredictorConfig& config,
      const base::FilePath& model_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      WARN_UNUSED_RESULT;

  // Fills in a boolean decision given a RankerExample. Returns false if a
  // prediction could not be made (e.g. the model is not loaded yet).
  bool Predict(const RankerExample& example,
               bool* prediction) WARN_UNUSED_RESULT;

  // Returns a score between 0 and 1. Returns false if a
  // prediction could not be made (e.g. the model is not loaded yet).
  bool PredictScore(const RankerExample& example,
                    float* prediction) WARN_UNUSED_RESULT;

  // Validates that the loaded RankerModel is a valid BinaryClassifier model.
  static RankerModelStatus ValidateModel(const RankerModel& model);

 protected:
  // Instatiates the inference module.
  bool Initialize() override;

 private:
  friend class BinaryClassifierPredictorTest;
  BinaryClassifierPredictor(const PredictorConfig& config);

  // TODO(hamelphi): Use an abstract BinaryClassifierInferenceModule in order to
  // generalize to other models.
  std::unique_ptr<GenericLogisticRegressionInference> inference_module_;

  DISALLOW_COPY_AND_ASSIGN(BinaryClassifierPredictor);
};

}  // namespace assist_ranker
#endif  // COMPONENTS_ASSIST_RANKER_BINARY_CLASSIFIER_PREDICTOR_H_
