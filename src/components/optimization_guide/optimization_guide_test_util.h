// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TEST_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TEST_UTIL_H_

#include <memory>

#include "components/optimization_guide/proto/models.pb.h"

// Returns a decision tree model with |threshold|, |weight|, and a single
// uninitialized node.
std::unique_ptr<optimization_guide::proto::PredictionModel>
GetMinimalDecisionTreePredictionModel(double threshold, double weight);

// Returns a decision tree model with |threshold|, |weight|, and a single
// leaf node with |leaf_value|.
std::unique_ptr<optimization_guide::proto::PredictionModel>
GetSingleLeafDecisionTreePredictionModel(double threshold,
                                         double weight,
                                         double leaf_value);

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TEST_UTIL_H_
