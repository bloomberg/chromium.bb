// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_RANDOM_TREE_TRAINER_H_
#define MEDIA_LEARNING_IMPL_RANDOM_TREE_TRAINER_H_

#include <limits>
#include <map>
#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

// Trains RandomTree decision tree classifier (doesn't handle regression).
//
// Decision trees, including RandomTree, classify instances as follows.  Each
// non-leaf node is marked with a feature number |i|.  The value of the |i|-th
// feature of the instance is then used to select which outgoing edge is
// traversed.  This repeats until arriving at a leaf, which has a distribution
// over target values that is the prediction.  The tree structure, including the
// feature index at each node and distribution at each leaf, is chosen once when
// the tree is trained.
//
// Training involves starting with a set of training examples, each of which has
// features and a target value.  The tree is constructed recursively, starting
// with the root.  For the node being constructed, the training algorithm is
// given the portion of the training set that would reach the node, if it were
// sent down the tree in a similar fashion as described above.  It then
// considers assigning each (unused) feature index as the index to split the
// training examples at this node.  For each index |t|, it groups the training
// set into subsets, each of which consists of those examples with the same
// of the |i|-th feature.  It then computes a score for the split using the
// target values that ended up in each group.  The index with the best score is
// chosen for the split.
//
// The training algorithm then recurses to build child nodes.  One child node is
// created for each observed value of the |i|-th feature in the training set.
// The child node is trained using the subset of the training set that shares
// that node's value for feature |i|.
//
// The above is a generic decision tree training algorithm.  A RandomTree
// differs from that mostly in how it selects the feature to split at each node
// during training.  Rather than computing a score for each feature, a
// RandomTree chooses a random subset of the features and only compares those.
//
// See https://en.wikipedia.org/wiki/Random_forest for information.  Note that
// this is just a single tree, not the whole forest.
//
// TODO(liberato): Right now, it not-so-randomly selects from the entire set.
// TODO(liberato): consider PRF or other simplified approximations.
// TODO(liberato): separate Model and TrainingAlgorithm.  This is the latter.
class COMPONENT_EXPORT(LEARNING_IMPL) RandomTreeTrainer {
 public:
  RandomTreeTrainer();
  ~RandomTreeTrainer();

  // Return a callback that can be used to train a random tree.
  static TrainingAlgorithmCB GetTrainingAlgorithmCB();

  std::unique_ptr<Model> Train(const TrainingData& examples);

 private:
  // Set of feature indices.
  using FeatureSet = std::set<int>;

  // Information about a proposed split, and the training sets that would result
  // from that split.
  struct Split {
    Split();
    explicit Split(int index);
    Split(Split&& rhs);
    ~Split();

    Split& operator=(Split&& rhs);

    // Feature index to split on.
    size_t split_index = 0;

    // Expected nats needed to compute the class, given that we're at this
    // node in the tree.
    // "nat" == entropy measured with natural log rather than base-2.
    double nats_remaining = std::numeric_limits<double>::infinity();

    // Per-branch (i.e. per-child node) information about this split.
    struct BranchInfo {
      explicit BranchInfo(scoped_refptr<TrainingDataStorage> storage);
      BranchInfo(const BranchInfo& rhs) = delete;
      BranchInfo(BranchInfo&& rhs);
      ~BranchInfo();

      BranchInfo& operator=(const BranchInfo& rhs) = delete;
      BranchInfo& operator=(BranchInfo&& rhs) = delete;

      // Training set for this branch of the split.
      TrainingData training_data;

      // Number of occurances of each target value in |training_data| along this
      // branch of the split.
      // This is a flat_map since we're likely to have a very small (e.g.,
      // "true / "false") number of targets.
      base::flat_map<TargetValue, int> class_counts;
    };

    // [feature value at this split] = info about which examples take this
    // branch of the split.
    std::map<FeatureValue, BranchInfo> branch_infos;

    DISALLOW_COPY_AND_ASSIGN(Split);
  };

  // Build this node from |training_data|.  |used_set| is the set of features
  // that we already used higher in the tree.
  std::unique_ptr<Model> Build(const TrainingData& training_data,
                               const FeatureSet& used_set);

  // Compute and return a split of |training_data| on the |index|-th feature.
  Split ConstructSplit(const TrainingData& training_data, int index);

  DISALLOW_COPY_AND_ASSIGN(RandomTreeTrainer);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_RANDOM_TREE_TRAINER_H_
