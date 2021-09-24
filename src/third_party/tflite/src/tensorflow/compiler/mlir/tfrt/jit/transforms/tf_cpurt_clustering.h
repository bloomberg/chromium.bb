/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_COMPILER_MLIR_TFRT_JIT_TF_CPURT_CLUSTERING_H_
#define TENSORFLOW_COMPILER_MLIR_TFRT_JIT_TF_CPURT_CLUSTERING_H_

#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/transforms/cluster_ops_by_policy.h"

namespace tensorflow {

// This is a temporary control flag to gradually enable compilation for
// operations based on the correctness and performance confidence. For example
// Tier 1 operations are simple enough and well tested, so they can be safely
// enabled for all models. We'll be introducing new tiers based on the
// completeness of lowering and testing, and eventually will remove this flag.
enum class CpurtClusteringTier : uint8_t {
  // All cwise operations (unary, binary, ternary) plus a tf.Transpose.
  kTier1 = 0,

  // TODO(ezhulenev): Include metadata (shape, reshape) and slicing into tier 2?
  // TODO(ezhulenev): Include reductions into tier 3?

  // All operations that do have clustering policy.
  kAll = 1
};

// Adds policies for clustering operations for TF->CPURT JIT compilation.
void populateTfCpurtClusteringPolicies(
    mlir::TFDevice::ClusteringPolicySet& policies,
    CpurtClusteringTier tier = CpurtClusteringTier::kAll);

// Adds policies for propagating constraints through Tensorflow operations. We
// do not add `tf.Const` operations to the clusters, however before compilation
// we sink some of them into the cluster body, and to properly verify compiled
// function body and infer operands constraints we need a policy for constants.
void populateTfCpurtConstraintsPolicies(
    mlir::TFDevice::ClusteringPolicySet& policies,
    CpurtClusteringTier tier = CpurtClusteringTier::kAll);

// Returns success if constant value can be sunk into the compiled function. We
// currently only support small integer constants that typically correspond to
// the reduction dimension, transpose permutation and other similar values that
// are required for successful compilation.
//
// We prefer to keep large constants as `tf.Const` operations outside of the
// compiled regions, and rely on the runtime to instantiate them as tensors.
mlir::LogicalResult IsCompilableConstant(mlir::ElementsAttr value);

// Verifies that discovered operations cluster satisfies TF->CPURT JIT
// compilation constraints.
mlir::LogicalResult VerifyCluster(const mlir::TFDevice::Cluster& cluster);

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFRT_JIT_TF_CPURT_CLUSTERING_H_
