// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_PREDICTOR_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_PREDICTOR_FACTORY_H_

#include "third_party/blink/public/platform/input/input_predictor.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

namespace input_prediction {

enum class PredictorType {
  kScrollPredictorTypeLsq,
  kScrollPredictorTypeKalman,
  kScrollPredictorTypeLinearFirst,
  kScrollPredictorTypeLinearSecond,
  kScrollPredictorTypeLinearResampling,
  kScrollPredictorTypeEmpty
};
}  // namespace input_prediction

class BLINK_PLATFORM_EXPORT PredictorFactory {
 public:
  // Returns the PredictorType associated to the given predictor
  // name if found, otherwise returns kScrollPredictorTypeEmpty
  static input_prediction::PredictorType GetPredictorTypeFromName(
      const std::string& predictor_name);

  // Returns the predictor designed by its type if found, otherwise returns
  // PredictorEmpty
  static std::unique_ptr<InputPredictor> GetPredictor(
      input_prediction::PredictorType predictor_type);

  // Returns the feature enabled kalman predictor options
  static unsigned int GetKalmanPredictorOptions();

  // Predictor options cache
  static unsigned int predictor_options_;

 private:
  PredictorFactory() = delete;
  ~PredictorFactory() = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_PREDICTOR_FACTORY_H_
