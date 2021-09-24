/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_GPU_NCCL_ALL_GATHER_THUNK_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_GPU_NCCL_ALL_GATHER_THUNK_H_

#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/mhlo/IR/lhlo_ops.h"
#include "tensorflow/compiler/xla/service/collective_ops_utils.h"
#include "tensorflow/compiler/xla/service/gpu/buffer_allocations.h"
#include "tensorflow/compiler/xla/service/gpu/nccl_collective_thunk.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/platform/types.h"

namespace xla {
namespace gpu {

struct NcclAllGatherConfig {
  NcclCollectiveConfig config;
};

// Thunk that performs a NCCL-based All-Gather among CUDA GPU-based replicas.
class NcclAllGatherThunk : public NcclCollectiveThunk {
 public:
  NcclAllGatherThunk(ThunkInfo thunk_info, mlir::lmhlo::AllGatherOp op,
                     std::vector<Buffer> buffers);

  // Returns whether the given instruction can be lowered to a nccl all-gather
  // call.
  static bool CanImplement(mlir::lmhlo::AllGatherOp op);

  static const char* GetName() { return "AllGather"; }

  static bool IsDegenerate(mlir::lmhlo::AllGatherOp op, int64_t replica_count,
                           int64_t partition_count) {
    return GetNcclAllGatherConfig(op).config.IsDegenerate(replica_count,
                                                          partition_count);
  }

  static CollectiveOpGroupMode GetGroupMode(mlir::lmhlo::AllGatherOp op) {
    return GetNcclAllGatherConfig(op).config.group_mode;
  }

 protected:
  Status RunNcclCollective(const ExecuteParams& params,
                           ncclComm_t comm) override;

  const NcclCollectiveConfig& config() const override { return config_.config; }

 private:
  static NcclAllGatherConfig GetNcclAllGatherConfig(
      mlir::lmhlo::AllGatherOp op);

  const NcclAllGatherConfig config_;
  const std::vector<Buffer> buffers_;
};

}  // namespace gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_GPU_NCCL_ALL_GATHER_THUNK_H_
