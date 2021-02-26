// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_test_util.h"

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetMinimalDecisionTreePredictionModel(double threshold, double weight) {
  auto prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(threshold);
  optimization_guide::proto::DecisionTree decision_tree_model;
  decision_tree_model.set_weight(weight);

  optimization_guide::proto::TreeNode* tree_node =
      decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(0);

  *prediction_model->mutable_model()->mutable_decision_tree() =
      decision_tree_model;

  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetSingleLeafDecisionTreePredictionModel(double threshold,
                                         double weight,
                                         double leaf_value) {
  auto prediction_model =
      GetMinimalDecisionTreePredictionModel(threshold, weight);
  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_leaf()
      ->mutable_vector()
      ->add_value()
      ->set_double_value(leaf_value);
  return prediction_model;
}
