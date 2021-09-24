/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/xla/service/sharding_propagation.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "tensorflow/compiler/xla/protobuf_util.h"
#include "tensorflow/compiler/xla/service/dot_as_convolution_util.h"
#include "tensorflow/compiler/xla/service/hlo_casting_utils.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_graph_dumper.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_instructions.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/hlo_sharding.h"
#include "tensorflow/compiler/xla/service/hlo_sharding_metadata.h"
#include "tensorflow/compiler/xla/service/hlo_sharding_util.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"

namespace xla {
namespace {

using ComputationMap =
    absl::flat_hash_map<const HloComputation*, HloInstruction*>;

// Returns true iff the specified hlo or sharding has a spatially partitioned
// sharding (tiled or replicated) what can be propagated by sharding
// propagation.
bool IsSpatiallyPartitioned(const HloSharding& sharding) {
  if (sharding.IsTuple()) {
    return absl::c_any_of(sharding.tuple_elements(), IsSpatiallyPartitioned);
  } else {
    return !sharding.IsTileMaximal() || sharding.IsReplicated();
  }
}
bool IsSpatiallyPartitioned(const HloInstruction* hlo) {
  return hlo->has_sharding() && IsSpatiallyPartitioned(hlo->sharding());
}

// Updates the sharding of the specified instruction with the specified sharding
// if it is better than the current one and returns true if a new sharding have
// been applied. If may_combine_partial_sharding is true, this may combine the
// new and existing sharding if they are both partial tiling partial
// replication.
bool MaybeImproveInstructionSharding(HloSharding sharding,
                                     HloInstruction* instruction,
                                     bool may_combine_partial_sharding) {
  // We don't want to propagate tile maximal shardings.
  if (!IsSpatiallyPartitioned(sharding)) {
    return false;
  }
  // Any sharding is better then no sharding.
  if (!instruction->has_sharding()) {
    instruction->set_sharding(std::move(sharding));
    return true;
  }
  int64_t sharding_tiles = sharding.NumTiles();
  if (hlo_sharding_util::MergeSharding(instruction->sharding(), &sharding,
                                       may_combine_partial_sharding)) {
    // Override existing tiled sharding only when the new sharding is compatible
    // with the existing one. This avoids unexpected resharding when `sharding`
    // just has more tiles than existing sharding but they are not mergeable.
    if (instruction->shape().IsArray() &&
        !instruction->sharding().IsTileMaximal() &&
        sharding.NumTiles() == sharding_tiles) {
      std::vector<int64_t> diff_dims;
      for (int64_t i = 0; i < instruction->shape().rank(); ++i) {
        if (instruction->sharding().tile_assignment().dim(i) ==
            sharding.tile_assignment().dim(i)) {
          continue;
        }
        if (instruction->sharding().tile_assignment().dim(i) != 1) {
          return false;
        }
        diff_dims.push_back(i);
      }
      if (hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
              sharding, diff_dims) != instruction->sharding()) {
        return false;
      }
    }
    instruction->set_sharding(std::move(sharding));
    return true;
  }
  return false;
}

// We consider a convolution kernel to be small iff it is smaller along all
// spatial dimensions then the output of the convolution. The rational is that
// we can either shard the kernel or the output and we want to shard the larger
// one for better efficiency.
bool IsConvolutionKernelSmall(const HloInstruction* instruction) {
  CHECK_EQ(instruction->opcode(), HloOpcode::kConvolution);
  const HloInstruction* rhs = instruction->operand(1);
  const auto& dnums = instruction->convolution_dimension_numbers();
  int64_t kernel_dim_prod = 1;
  int64_t output_dim_prod = 1;
  for (int64_t i = 0; i < dnums.input_spatial_dimensions().size(); ++i) {
    int64_t kernel_dim =
        rhs->shape().dimensions(dnums.kernel_spatial_dimensions(i));
    kernel_dim_prod *= kernel_dim;
    int64_t output_dim =
        instruction->shape().dimensions(dnums.output_spatial_dimensions(i));
    output_dim_prod *= output_dim;
    if (kernel_dim >= output_dim &&
        (i < 2 || kernel_dim > 3 || kernel_dim_prod >= output_dim_prod)) {
      return false;
    }
  }
  return true;
}

// Return the operand which is the most suitable for determining the sharding
// for the specified instruction or nullptr if there isn't any suitable operand.
const HloInstruction* PickRepresentativeOperand(
    const HloInstruction* instruction) {
  switch (instruction->opcode()) {
    case HloOpcode::kMap:
    case HloOpcode::kPad:
    case HloOpcode::kPower:
    case HloOpcode::kReverse:
    case HloOpcode::kSlice:
    case HloOpcode::kShiftLeft:
    case HloOpcode::kShiftRightArithmetic:
    case HloOpcode::kShiftRightLogical:
      // For these opcodes the output sharding has to be determined by the
      // sharding of the first operand but we can only determine sharding based
      // on it if it already has a sharding.
      if (instruction->operand(0)->has_sharding()) {
        return instruction->operand(0);
      }
      return nullptr;
    case HloOpcode::kAbs:
    case HloOpcode::kAdd:
    case HloOpcode::kAnd:
    case HloOpcode::kAtan2:
    case HloOpcode::kBitcastConvert:
    case HloOpcode::kCeil:
    case HloOpcode::kClamp:
    case HloOpcode::kClz:
    case HloOpcode::kCompare:
    case HloOpcode::kComplex:
    case HloOpcode::kConcatenate:
    case HloOpcode::kConvert:
    case HloOpcode::kCopy:
    case HloOpcode::kCos:
    case HloOpcode::kAllGather:
    case HloOpcode::kAllReduce:
    case HloOpcode::kReduceScatter:
    case HloOpcode::kAllToAll:
    case HloOpcode::kCollectivePermute:
    case HloOpcode::kDivide:
    case HloOpcode::kExp:
    case HloOpcode::kExpm1:
    case HloOpcode::kFloor:
    case HloOpcode::kImag:
    case HloOpcode::kIsFinite:
    case HloOpcode::kLog:
    case HloOpcode::kLog1p:
    case HloOpcode::kLogistic:
    case HloOpcode::kMaximum:
    case HloOpcode::kMinimum:
    case HloOpcode::kMultiply:
    case HloOpcode::kNegate:
    case HloOpcode::kNot:
    case HloOpcode::kOr:
    case HloOpcode::kPopulationCount:
    case HloOpcode::kReal:
    case HloOpcode::kReducePrecision:
    case HloOpcode::kRemainder:
    case HloOpcode::kRoundNearestAfz:
    case HloOpcode::kRsqrt:
    case HloOpcode::kSelect:
    case HloOpcode::kSign:
    case HloOpcode::kSin:
    case HloOpcode::kSort:
    case HloOpcode::kSqrt:
    case HloOpcode::kCbrt:
    case HloOpcode::kSubtract:
    case HloOpcode::kTanh:
    case HloOpcode::kTupleSelect:
    case HloOpcode::kWhile:
    case HloOpcode::kXor: {
      // For these opcodes the output sharding can be determined by any operand
      // so we find the operand with the most specific sharding.
      const HloInstruction* best_operand = nullptr;
      for (const HloInstruction* operand : instruction->operands()) {
        if (operand->has_sharding() &&
            (best_operand == nullptr ||
             hlo_sharding_util::IsShardingMoreSpecific(
                 operand->sharding(), best_operand->sharding()))) {
          best_operand = operand;
        }
      }
      return best_operand;
    }

    // There is no suitable operand for the rest of the opcodes.
    case HloOpcode::kAddDependency:
    case HloOpcode::kAfterAll:
    case HloOpcode::kAllGatherStart:
    case HloOpcode::kAllGatherDone:
    case HloOpcode::kAllReduceStart:
    case HloOpcode::kAllReduceDone:
    case HloOpcode::kBatchNormGrad:
    case HloOpcode::kBatchNormInference:
    case HloOpcode::kBatchNormTraining:
    case HloOpcode::kBitcast:
    case HloOpcode::kBroadcast:
    case HloOpcode::kCall:
    case HloOpcode::kCholesky:
    case HloOpcode::kCollectivePermuteDone:
    case HloOpcode::kCollectivePermuteStart:
    case HloOpcode::kConditional:
    case HloOpcode::kConstant:
    case HloOpcode::kConvolution:
    case HloOpcode::kCopyDone:
    case HloOpcode::kCopyStart:
    case HloOpcode::kCustomCall:
    case HloOpcode::kDomain:
    case HloOpcode::kDot:
    case HloOpcode::kDynamicSlice:
    case HloOpcode::kDynamicUpdateSlice:
    case HloOpcode::kDynamicReshape:
    case HloOpcode::kFft:
    case HloOpcode::kFusion:
    case HloOpcode::kGather:
    case HloOpcode::kGetTupleElement:
    case HloOpcode::kInfeed:
    case HloOpcode::kIota:
    case HloOpcode::kOutfeed:
    case HloOpcode::kParameter:
    case HloOpcode::kPartitionId:
    case HloOpcode::kRecv:
    case HloOpcode::kRecvDone:
    case HloOpcode::kReduce:
    case HloOpcode::kReduceWindow:
    case HloOpcode::kReplicaId:
    case HloOpcode::kReshape:
    case HloOpcode::kRng:
    case HloOpcode::kRngGetAndUpdateState:
    case HloOpcode::kRngBitGenerator:
    case HloOpcode::kScatter:
    case HloOpcode::kSelectAndScatter:
    case HloOpcode::kSend:
    case HloOpcode::kSendDone:
    case HloOpcode::kTrace:
    case HloOpcode::kTranspose:
    case HloOpcode::kTriangularSolve:
    case HloOpcode::kTuple:
    case HloOpcode::kGetDimensionSize:
    case HloOpcode::kSetDimensionSize:
      return nullptr;
  }
}

bool SupportSpatialPartitioning(const HloInstruction* instruction,
                                const ComputationMap& computation_map,
                                bool is_spmd) {
  if (instruction->parent()->root_instruction() == instruction &&
      computation_map.find(instruction->parent()) == computation_map.end()) {
    // We don't support sharding the root instruction of a computation yet,
    // unless the computation is a while body.
    return false;
  }

  if (instruction->IsElementwise() &&
      (instruction->opcode() != HloOpcode::kRng || is_spmd)) {
    return true;
  }
  switch (instruction->opcode()) {
    case HloOpcode::kBroadcast:
    case HloOpcode::kConcatenate:
    case HloOpcode::kConditional:
    case HloOpcode::kConstant:
    case HloOpcode::kConvolution:
    case HloOpcode::kDot:
    case HloOpcode::kDynamicSlice:
    case HloOpcode::kDynamicUpdateSlice:
    case HloOpcode::kGather:
    case HloOpcode::kGetTupleElement:
    case HloOpcode::kInfeed:
    case HloOpcode::kIota:
    case HloOpcode::kPad:
    case HloOpcode::kReduceWindow:
    case HloOpcode::kReshape:
    case HloOpcode::kScatter:
    case HloOpcode::kSelectAndScatter:
    case HloOpcode::kSlice:
    case HloOpcode::kSort:
    case HloOpcode::kTranspose:
    case HloOpcode::kTuple:
    case HloOpcode::kWhile:
    case HloOpcode::kReduce:
    case HloOpcode::kRngBitGenerator:
      return true;
    case HloOpcode::kAllReduce:
    case HloOpcode::kReduceScatter:
      // Only if channel_id is not specified.
      return instruction->channel_id() == absl::nullopt;
    case HloOpcode::kParameter:
      return computation_map.find(instruction->parent()) !=
             computation_map.end();
    case HloOpcode::kReverse:
      return is_spmd;
    default:
      return false;
  }
}

bool InferDotShardingFromOperands(
    HloInstruction* instruction,
    const dot_as_convolution_util::DotConvolutionDimsInfo& dnums,
    bool may_combine_partial_sharding) {
  auto from_operand = [&](int64_t operand_index) {
    auto operand = instruction->operand(operand_index);
    const HloSharding& operand_sharding = operand->sharding();
    if (operand_sharding.IsTileMaximal()) {
      return operand_sharding;
    }
    std::vector<int64_t> contracting_dims;
    contracting_dims.reserve(dnums.contracting_dims.size());
    for (const auto& dim : dnums.contracting_dims) {
      contracting_dims.push_back(operand_index == 0 ? dim.lhs : dim.rhs);
    }
    // It's possible that some size-1 spatial dims of convolutions are parsed as
    // non-contracting dims. We might have tiled dimensions on them.
    for (const auto& dim : operand_index == 0
                               ? dnums.rhs_non_contracting_dims
                               : dnums.lhs_non_contracting_dims) {
      int64_t d = operand_index == 0 ? dim.lhs : dim.rhs;
      if (d > 0) {
        contracting_dims.push_back(d);
      }
    }
    auto replicate_contracting_dims =
        hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
            operand_sharding, contracting_dims);
    std::vector<int64_t> out_dims_to_op_perm(instruction->shape().rank(), -1);
    std::vector<int64_t> op_dims_to_output_perm(operand->shape().rank(), -1);
    for (const auto& dim : dnums.batch_dims) {
      out_dims_to_op_perm[dim.output] = operand_index == 0 ? dim.lhs : dim.rhs;
      op_dims_to_output_perm[operand_index == 0 ? dim.lhs : dim.rhs] =
          dim.output;
    }
    for (const auto& dim : operand_index == 0
                               ? dnums.lhs_non_contracting_dims
                               : dnums.rhs_non_contracting_dims) {
      out_dims_to_op_perm[dim.output] = operand_index == 0 ? dim.lhs : dim.rhs;
      op_dims_to_output_perm[operand_index == 0 ? dim.lhs : dim.rhs] =
          dim.output;
    }
    return *hlo_sharding_util::TransposeShardingWithCollapsedDims(
        replicate_contracting_dims, op_dims_to_output_perm,
        out_dims_to_op_perm);
  };
  bool changed = false;
  int64_t larger_operand =
      ShapeUtil::ByteSizeOf(instruction->operand(0)->shape()) >=
              ShapeUtil::ByteSizeOf(instruction->operand(1)->shape())
          ? 0
          : 1;
  if (IsSpatiallyPartitioned(instruction->operand(larger_operand))) {
    changed |= MaybeImproveInstructionSharding(from_operand(larger_operand),
                                               instruction,
                                               may_combine_partial_sharding);
  }
  if (IsSpatiallyPartitioned(instruction->operand(1 - larger_operand))) {
    changed |= MaybeImproveInstructionSharding(from_operand(1 - larger_operand),
                                               instruction,
                                               may_combine_partial_sharding);
  }
  return changed;
}

bool InferGatherParallelShardingFromOperands(
    HloInstruction* instruction,
    const hlo_sharding_util::GatherParallelDims& parallel_dims,
    bool may_combine_partial_sharding) {
  auto from_operand = [instruction](
                          int64_t operand_index,
                          absl::Span<const int64_t>
                              output_aligned_parallel_dims,
                          absl::Span<const int64_t> output_parallel_dims) {
    const HloInstruction* operand = instruction->operand(operand_index);
    const HloSharding& operand_sharding = operand->sharding();
    if (operand_sharding.IsTileMaximal()) {
      return operand_sharding;
    }
    auto dnums = instruction->gather_dimension_numbers();
    std::vector<int64_t> output_tile_dims(instruction->shape().rank(), 1);
    std::vector<int64_t> index_non_parallel_dims;
    index_non_parallel_dims.reserve(operand->shape().rank());
    // Detect non parallel dimensions in the index.
    for (int i = 0; i < operand->shape().rank(); ++i) {
      if (!absl::c_linear_search(output_aligned_parallel_dims, i)) {
        index_non_parallel_dims.push_back(i);
      }
    }
    // Collect tile dimensions in the operand. The order of the parallel
    // dimensions in output_aligned_parallel_dims is the same as that of the
    // output
    for (int i = 0; i < output_aligned_parallel_dims.size(); ++i) {
      const int64_t indices_idx = output_aligned_parallel_dims[i];
      const int64_t output_idx = output_parallel_dims[i];
      output_tile_dims[output_idx] =
          operand_sharding.tile_assignment().dim(indices_idx);
    }
    HloSharding replicate_non_parallel_dims =
        hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
            operand_sharding, index_non_parallel_dims);
    if (replicate_non_parallel_dims.NumTiles() == 1) {
      return replicate_non_parallel_dims;
    }
    for (int64_t i = replicate_non_parallel_dims.TiledDataRank();
         i < replicate_non_parallel_dims.tile_assignment().num_dimensions();
         ++i) {
      output_tile_dims.push_back(
          replicate_non_parallel_dims.tile_assignment().dim(i));
    }
    auto output_tile_assignment = replicate_non_parallel_dims.tile_assignment();
    output_tile_assignment.Reshape(output_tile_dims);
    return replicate_non_parallel_dims.ReplicateOnLastTileDim()
               ? HloSharding::PartialTile(
                     output_tile_assignment,
                     replicate_non_parallel_dims.metadata())
               : HloSharding::Subgroup(
                     output_tile_assignment,
                     replicate_non_parallel_dims.subgroup_types(),
                     replicate_non_parallel_dims.metadata());
  };

  bool changed = false;
  auto output_parallel_dims =
      hlo_sharding_util::GatherParallelOutputDims(*instruction, parallel_dims);
  if (IsSpatiallyPartitioned(instruction->operand(0))) {
    changed |= MaybeImproveInstructionSharding(
        from_operand(
            0,
            absl::MakeConstSpan(
                hlo_sharding_util::GatherOutputAlignedOperandParallelDims(
                    *instruction, parallel_dims)),
            absl::MakeConstSpan(output_parallel_dims)),
        instruction, may_combine_partial_sharding);
  }
  if (IsSpatiallyPartitioned(instruction->operand(1))) {
    changed |= MaybeImproveInstructionSharding(
        from_operand(1,
                     absl::MakeConstSpan(parallel_dims.indices_parallel_dims),
                     absl::MakeConstSpan(output_parallel_dims)),
        instruction, may_combine_partial_sharding);
  }
  return changed;
}

// Convolution handling for InferShardingFromOperands().
bool InferConvolutionShardingFromOperands(HloInstruction* instruction,
                                          int64_t aggressiveness,
                                          bool may_combine_partial_sharding) {
  auto get_partitions_for_dims =
      [&](const HloInstruction* inst,
          absl::Span<
              const dot_as_convolution_util::DotConvolutionDimsInfo::DimNums>
              dims,
          int lhs_or_rhs) {
        int64_t partitions = 1;
        if (!inst->has_sharding()) {
          return partitions;
        }
        const auto& sharding = inst->sharding();
        if (sharding.IsTileMaximal()) {
          return partitions;
        }
        for (const auto& dim : dims) {
          if (lhs_or_rhs == 0) {
            partitions *= sharding.tile_assignment().dim(dim.lhs);
          } else {
            CHECK_EQ(lhs_or_rhs, 1);
            partitions *= sharding.tile_assignment().dim(dim.rhs);
          }
        }
        return partitions;
      };
  auto dot_dims =
      dot_as_convolution_util::ParseConvolutionDimsInfo(instruction);
  const int64_t lhs_conv_spatial_partitions = get_partitions_for_dims(
      instruction->operand(0), dot_dims.conv_spatial_dims, 0);
  const int64_t rhs_conv_spatial_partitions = get_partitions_for_dims(
      instruction->operand(1), dot_dims.conv_spatial_dims, 1);
  if (dot_dims.conv_spatial_dims.empty() ||
      (lhs_conv_spatial_partitions == 1 && rhs_conv_spatial_partitions == 1 &&
       instruction->batch_group_count() == 1 &&
       instruction->feature_group_count() == 1)) {
    return InferDotShardingFromOperands(instruction, dot_dims,
                                        may_combine_partial_sharding);
  }
  const auto& dnums = instruction->convolution_dimension_numbers();
  const HloInstruction* lhs = instruction->operand(0);
  auto get_tiled_sharding_based_on_lhs = [&] {
    CHECK(!lhs->sharding().IsTileMaximal());
    std::vector<int64_t> output_to_lhs_indices(instruction->shape().rank());
    output_to_lhs_indices[dnums.output_batch_dimension()] =
        dnums.input_batch_dimension();
    output_to_lhs_indices[dnums.output_feature_dimension()] =
        dnums.input_feature_dimension();
    for (int64_t i = 0; i < dnums.input_spatial_dimensions_size(); ++i) {
      output_to_lhs_indices[dnums.output_spatial_dimensions(i)] =
          dnums.input_spatial_dimensions(i);
    }
    return hlo_sharding_util::TransposeSharding(lhs->sharding(),
                                                output_to_lhs_indices);
  };
  if (!IsSpatiallyPartitioned(lhs)) {
    return false;
  }
  if (lhs->sharding().NumTiles() == 1) {
    return MaybeImproveInstructionSharding(lhs->sharding(), instruction,
                                           may_combine_partial_sharding);
  }

  if (IsConvolutionKernelSmall(instruction)) {
    // If the kernel is small compared to the input then we can generate an
    // output what is sharded the same way as the input.
    const auto& tile_assignment = lhs->sharding().tile_assignment();
    if (tile_assignment.dim(dnums.input_feature_dimension()) > 1) {
      return false;
    }
    return MaybeImproveInstructionSharding(get_tiled_sharding_based_on_lhs(),
                                           instruction,
                                           may_combine_partial_sharding);
  }
  // If the kernel is large (e.g backward convolution) then we only support
  // replicated output.
  return MaybeImproveInstructionSharding(
      hlo_sharding_util::ReplicateAllDataDims(lhs->sharding()), instruction,
      may_combine_partial_sharding);
}

bool CanPropagateThroughAtAgressiveLevel(const HloInstruction& inst,
                                         int64_t aggressiveness) {
  // At minimum agressiveness, only allow pass-through ops.
  if (aggressiveness < 1 && !inst.IsElementwise() &&
      inst.opcode() != HloOpcode::kTranspose &&
      inst.opcode() != HloOpcode::kReshape &&
      inst.opcode() != HloOpcode::kTuple &&
      inst.opcode() != HloOpcode::kGetTupleElement &&
      inst.opcode() != HloOpcode::kWhile) {
    return false;
  }
  // Broadcast propagation should have at least aggressiveness 2.
  if (aggressiveness < 2 && inst.opcode() == HloOpcode::kBroadcast) {
    return false;
  }
  return true;
}

// Tries to update the sharding of the specified instruction based on its
// operands and returns true if the sharding of the instruction have been
// changed and false otherwise.
bool InferShardingFromOperands(HloInstruction* instruction,
                               const ComputationMap& computation_map,
                               bool is_spmd, int64_t aggressiveness) {
  if (!CanPropagateThroughAtAgressiveLevel(*instruction, aggressiveness)) {
    return false;
  }
  // Do not change manual sharding.
  if (instruction->has_sharding() && instruction->sharding().IsManual()) {
    return false;
  }
  // Propagate manual sharding. Avoid tuple shaped HLOs that group independent
  // together. Reduce, ReduceWindow, and Sort can be tuples but the elements
  // are correlated, so we propagate manual sharding through them.
  if (!instruction->has_sharding() &&
      (instruction->shape().IsArray() ||
       instruction->opcode() == HloOpcode::kReduce ||
       instruction->opcode() == HloOpcode::kSort ||
       instruction->opcode() == HloOpcode::kReduceWindow)) {
    for (const HloInstruction* op : instruction->operands()) {
      if (!op->has_sharding() || !op->sharding().IsManual()) continue;
      instruction->set_sharding(HloSharding::Manual(op->sharding().metadata()));
      return true;
    }
  }
  const bool may_combine_partial_sharding = is_spmd && aggressiveness > 0;
  if (!SupportSpatialPartitioning(instruction, computation_map, is_spmd)) {
    // If an array shaped HLO doesn't support spatial partitioning but at least
    // one of its operand is replicated then we make the HLO replicated as well.
    if (instruction->shape().IsTuple() || instruction->operand_count() == 0 ||
        instruction == instruction->parent()->root_instruction() ||
        instruction->HasSideEffect()) {
      return false;
    }
    for (const HloInstruction* op : instruction->operands()) {
      if (op->has_sharding() && op->sharding().NumTiles() == 1 &&
          !op->sharding().HasUniqueDevice()) {
        return MaybeImproveInstructionSharding(op->sharding(), instruction,
                                               may_combine_partial_sharding);
      }
    }
    return false;
  }

  auto get_maybe_tuple_sharding = [&](HloSharding sharding) {
    if (instruction->shape().IsArray()) {
      return sharding;
    }
    std::vector<HloSharding> tuple(instruction->shape().tuple_shapes_size(),
                                   std::move(sharding));
    return HloSharding::Tuple(instruction->shape(), tuple);
  };

  switch (instruction->opcode()) {
    case HloOpcode::kGetTupleElement: {
      const HloInstruction* operand = instruction->operand(0);
      if (!IsSpatiallyPartitioned(operand)) {
        return false;
      }
      HloSharding new_sharding = operand->sharding().GetSubSharding(
          operand->shape(), {instruction->tuple_index()});
      return MaybeImproveInstructionSharding(
          std::move(new_sharding), instruction, may_combine_partial_sharding);
    }
    case HloOpcode::kTuple: {
      if (absl::c_none_of(instruction->operands(),
                          [](const HloInstruction* hlo) {
                            return IsSpatiallyPartitioned(hlo);
                          })) {
        // None of the operands have a spatially partitioned sharding.
        return false;
      }
      const Shape& shape = instruction->shape();
      bool changed = false;
      if (!instruction->has_sharding()) {
        // Set the sharding for all elements in the tuple because it isn't
        // possible to set a partial sharding.
        changed = true;
        for (int64_t i = 0; i < ShapeUtil::TupleElementCount(shape); ++i) {
          const HloInstruction* operand = instruction->operand(i);
          if (!operand->has_sharding()) {
            continue;
          }
          if (operand->sharding().IsTuple()) {
            if (operand->sharding().tuple_elements().empty()) {
              continue;
            }
            // Use ReplicateAllDataDims to to preserve manual subgroups.
            instruction->set_sharding(HloSharding::SingleTuple(
                instruction->shape(),
                hlo_sharding_util::ReplicateAllDataDims(
                    operand->sharding().tuple_elements()[0])
                    .WithoutMetadata()));
          } else {
            instruction->set_sharding(HloSharding::SingleTuple(
                instruction->shape(),
                hlo_sharding_util::ReplicateAllDataDims(operand->sharding())
                    .WithoutMetadata()));
          }
          break;
        }
      }
      if (!instruction->has_sharding()) {
        return false;
      }
      // Go through each operand and if the operand has a sharding that is
      // better than the current sharding for that tuple element then update
      // it.
      std::vector<HloSharding> sub_shardings =
          instruction->sharding().tuple_elements();
      int64_t sub_sharding_index = 0;
      for (int64_t i = 0; i < ShapeUtil::TupleElementCount(shape); ++i) {
        const HloInstruction* operand = instruction->operand(i);
        if (operand->has_sharding()) {
          if (operand->shape().IsTuple()) {
            for (int64_t i = 0, e = ShapeUtil::GetLeafCount(operand->shape());
                 i < e; ++i) {
              if (hlo_sharding_util::IsShardingMoreSpecific(
                      operand->sharding().tuple_elements()[i],
                      sub_shardings[sub_sharding_index + i])) {
                sub_shardings[sub_sharding_index + i] =
                    operand->sharding().tuple_elements()[i];
              }
            }
          } else {
            if (hlo_sharding_util::IsShardingMoreSpecific(
                    operand->sharding(), sub_shardings[sub_sharding_index])) {
              sub_shardings[sub_sharding_index] = operand->sharding();
            }
          }
        }
        sub_sharding_index += ShapeUtil::GetLeafCount(operand->shape());
      }

      HloSharding new_sharding = HloSharding::Tuple(shape, sub_shardings);
      if (new_sharding != instruction->sharding()) {
        instruction->set_sharding(std::move(new_sharding));
        return true;
      }
      return changed;
    }
    case HloOpcode::kReduce: {
      // Reduce could have a tuple shape, where the first half of operands are
      // the arrays to reduce, and the second half of operands are the init
      // values.
      bool changed = false;
      auto* reduce = Cast<HloReduceInstruction>(instruction);
      for (HloInstruction* operand : reduce->inputs()) {
        if (!IsSpatiallyPartitioned(operand)) {
          continue;
        }
        if (operand->sharding().IsReplicated() ||
            (!is_spmd &&
             absl::c_any_of(instruction->dimensions(), [operand](int64_t dim) {
               return operand->sharding().tile_assignment().dim(dim) > 1;
             }))) {
          // We are reducing along one of the sharded dimensions. We only
          // support this in SPMD.
          changed |= MaybeImproveInstructionSharding(
              get_maybe_tuple_sharding(
                  hlo_sharding_util::ReplicateAllDataDims(operand->sharding())),
              reduce, may_combine_partial_sharding);
          continue;
        }
        auto after_partial_replication =
            operand->sharding().IsReplicated()
                ? operand->sharding()
                : hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
                      operand->sharding(), reduce->dimensions());
        if (after_partial_replication.IsReplicated()) {
          changed |= MaybeImproveInstructionSharding(
              get_maybe_tuple_sharding(after_partial_replication), reduce,
              may_combine_partial_sharding);
          continue;
        }
        // Use the same sharding for all tuple elements, because they are part
        // of the same reduce instruction.
        HloSharding new_sharding =
            get_maybe_tuple_sharding(hlo_sharding_util::RemoveShapeDimensions(
                after_partial_replication, reduce->dimensions()));
        changed |= MaybeImproveInstructionSharding(
            std::move(new_sharding), reduce, may_combine_partial_sharding);
      }
      return changed;
    }
    case HloOpcode::kBroadcast: {
      // Make forward propagation through broadcast low priority to avoid
      // resharding after broadcast.
      if (aggressiveness < 3) {
        return false;
      }
      const HloInstruction* op = instruction->operand(0);
      if (!IsSpatiallyPartitioned(op) || op->sharding().IsReplicated()) {
        return false;
      }
      // The output will be tiled along the broadcasted dimension the same way
      // as the input for the broadcast while the other dimensions are kept
      // non-tiled.
      std::vector<int64_t> target_tile_assignment_dimensions;
      const auto& dimensions = instruction->dimensions();
      for (int64_t i = 0; i < instruction->shape().rank(); ++i) {
        auto it = absl::c_find(dimensions, i);
        if (it == dimensions.end()) {
          target_tile_assignment_dimensions.push_back(1);
        } else {
          const int64_t source_dim = std::distance(dimensions.begin(), it);
          target_tile_assignment_dimensions.push_back(
              op->sharding().tile_assignment().dim(source_dim));
        }
      }
      for (int64_t i = op->sharding().TiledDataRank();
           i < op->sharding().tile_assignment().num_dimensions(); ++i) {
        target_tile_assignment_dimensions.push_back(
            op->sharding().tile_assignment().dim(i));
      }
      Array<int64_t> new_tile_assignment = op->sharding().tile_assignment();
      new_tile_assignment.Reshape(target_tile_assignment_dimensions);
      HloSharding new_sharding =
          op->sharding().ReplicateOnLastTileDim()
              ? HloSharding::PartialTile(new_tile_assignment,
                                         op->sharding().metadata())
              : HloSharding::Subgroup(new_tile_assignment,
                                      op->sharding().subgroup_types(),
                                      op->sharding().metadata());
      return MaybeImproveInstructionSharding(
          std::move(new_sharding), instruction, may_combine_partial_sharding);
    }
    case HloOpcode::kConvolution:
      return InferConvolutionShardingFromOperands(instruction, aggressiveness,
                                                  may_combine_partial_sharding);
    case HloOpcode::kTranspose: {
      const HloInstruction* input = instruction->operand(0);
      if (!IsSpatiallyPartitioned(input)) {
        return false;
      }
      HloSharding sharding = hlo_sharding_util::TransposeSharding(
          input->sharding(), instruction->dimensions());
      return MaybeImproveInstructionSharding(std::move(sharding), instruction,
                                             may_combine_partial_sharding);
    }
    case HloOpcode::kReduceWindow: {
      auto* reduce_window = Cast<HloReduceWindowInstruction>(instruction);
      auto has_dilation = [](const WindowDimension& dimensions) {
        return dimensions.base_dilation() > 1 ||
               dimensions.window_dilation() > 1;
      };
      if (absl::c_any_of(instruction->window().dimensions(), has_dilation)) {
        VLOG(2) << "Not applying sharding to reduce window because dilatation "
                   "isn't supported yet: "
                << reduce_window->ToString();
        return false;
      }
      bool changed = false;
      for (HloInstruction* operand : reduce_window->inputs()) {
        if (!IsSpatiallyPartitioned(operand)) {
          continue;
        }
        changed |= MaybeImproveInstructionSharding(
            get_maybe_tuple_sharding(operand->sharding()), reduce_window,
            may_combine_partial_sharding);
      }
      return changed;
    }
    case HloOpcode::kSelectAndScatter: {
      // Shard according to first operand, as output keeps the same shape.
      const HloInstruction* lhs = instruction->operand(0);
      if (!IsSpatiallyPartitioned(lhs)) {
        return false;
      }

      auto has_base_dilation = [](const WindowDimension& dimensions) {
        return dimensions.base_dilation() > 1;
      };
      if (absl::c_any_of(instruction->window().dimensions(),
                         has_base_dilation)) {
        VLOG(2) << "Not applying sharding to select-and-scatter because "
                   "base dilation isn't supported yet: "
                << instruction->ToString();
        return false;
      }
      return MaybeImproveInstructionSharding(lhs->sharding(), instruction,
                                             may_combine_partial_sharding);
    }
    case HloOpcode::kReshape: {
      if (!IsSpatiallyPartitioned(instruction->operand(0))) {
        return false;
      }
      absl::optional<HloSharding> new_sharding =
          hlo_sharding_util::ReshapeSharding(
              instruction->operand(0)->shape(), instruction->shape(),
              instruction->operand(0)->sharding());
      if (new_sharding.has_value()) {
        return MaybeImproveInstructionSharding(std::move(*new_sharding),
                                               instruction,
                                               may_combine_partial_sharding);
      }
      return false;
    }
    case HloOpcode::kReverse: {
      if (!IsSpatiallyPartitioned(instruction->operand(0))) {
        return false;
      }
      return MaybeImproveInstructionSharding(
          hlo_sharding_util::ReverseSharding(
              instruction->operand(0)->sharding(), instruction->dimensions()),
          instruction, may_combine_partial_sharding);
    }
    case HloOpcode::kDot: {
      const auto& dnums =
          dot_as_convolution_util::ParseDotGeneralFromDot(instruction);
      return InferDotShardingFromOperands(instruction, dnums,
                                          may_combine_partial_sharding);
    }
    case HloOpcode::kParameter: {
      auto parent_it = computation_map.find(instruction->parent());
      if (parent_it == computation_map.end()) {
        return false;
      }
      const HloInstruction* parent = parent_it->second;
      switch (parent->opcode()) {
        case HloOpcode::kConditional: {
          for (int64_t i = 1; i < parent->operand_count(); ++i) {
            if (parent->called_computations()[i - 1] == instruction->parent()) {
              if (parent->operand(i)->has_sharding()) {
                return MaybeImproveInstructionSharding(
                    parent->operand(i)->sharding(), instruction,
                    may_combine_partial_sharding);
              }
              return false;
            }
          }
          return false;
        }
        default:
          return false;
      }
    }
    case HloOpcode::kSort: {
      const HloInstruction* operand = PickRepresentativeOperand(instruction);
      if (!operand || !IsSpatiallyPartitioned(operand)) {
        return false;
      }

      if (!operand->sharding().IsTileMaximal() &&
          operand->sharding().tile_assignment().dim(
              instruction->dimensions(0)) != 1) {
        // Doesn't support sharding the sorting dimension.
        return false;
      }

      if (instruction->shape().IsTuple()) {
        return MaybeImproveInstructionSharding(
            HloSharding::SingleTuple(instruction->shape(), operand->sharding()),
            instruction, may_combine_partial_sharding);
      } else {
        return MaybeImproveInstructionSharding(operand->sharding(), instruction,
                                               may_combine_partial_sharding);
      }
    }
    case HloOpcode::kDynamicSlice:
    case HloOpcode::kDynamicUpdateSlice: {
      auto propagate_slicing = [&]() {
        const HloInstruction* operand =
            instruction->opcode() == HloOpcode::kDynamicSlice
                ? instruction->operand(0)
                : instruction->operand(1);
        if (!IsSpatiallyPartitioned(operand)) {
          return false;
        }

        if (operand->sharding().NumTiles() == 1) {
          return MaybeImproveInstructionSharding(
              operand->sharding(), instruction, may_combine_partial_sharding);
        }

        const auto& tile_assignment = operand->sharding().tile_assignment();
        for (int64_t i = 0; i < instruction->shape().rank(); ++i) {
          if (tile_assignment.dim(i) > 1 &&
              instruction->shape().dimensions(i) !=
                  operand->shape().dimensions(i)) {
            return false;
          }
        }
        return MaybeImproveInstructionSharding(operand->sharding(), instruction,
                                               may_combine_partial_sharding);
      };
      auto propagate_base = [&]() {
        if (instruction->opcode() != HloOpcode::kDynamicUpdateSlice) {
          return false;
        }
        if (!IsSpatiallyPartitioned(instruction->operand(0))) {
          return false;
        }
        return MaybeImproveInstructionSharding(
            instruction->operand(0)->sharding(), instruction,
            may_combine_partial_sharding);
      };
      bool changed = propagate_slicing();
      changed |= propagate_base();
      return changed;
    }
    case HloOpcode::kGather: {
      bool changed = false;
      if (IsSpatiallyPartitioned(instruction->operand(1))) {
        HloSharding new_sharding = hlo_sharding_util::GatherOutputSharding(
            instruction->operand(1)->sharding(), instruction);
        changed |= MaybeImproveInstructionSharding(
            std::move(new_sharding), instruction, may_combine_partial_sharding);
      }
      if (is_spmd) {
        auto gather_parallel_dims =
            hlo_sharding_util::GetGatherBatchParallelDims(*instruction);
        if (gather_parallel_dims) {
          changed |= InferGatherParallelShardingFromOperands(
              instruction, *gather_parallel_dims, may_combine_partial_sharding);
        }
        if (IsSpatiallyPartitioned(instruction->operand(0))) {
          absl::Span<const int64_t> operand_parallel_dims;
          if (gather_parallel_dims) {
            operand_parallel_dims = absl::MakeConstSpan(
                gather_parallel_dims->operand_parallel_dims);
          }
          HloSharding filtered_operand_sharding =
              hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
                  instruction->operand(0)->sharding(), operand_parallel_dims);
          auto maybe_from_data =
              hlo_sharding_util::GatherOutputShardingFromDataOperand(
                  filtered_operand_sharding, *instruction, instruction->shape(),
                  instruction->operand(0)->shape());
          if (maybe_from_data) {
            changed |= MaybeImproveInstructionSharding(
                std::move(*maybe_from_data), instruction,
                may_combine_partial_sharding);
          }
        }
      }
      return changed;
    }
    case HloOpcode::kScatter: {
      bool changed = false;
      if (is_spmd && IsSpatiallyPartitioned(instruction->operand(0))) {
        changed |= MaybeImproveInstructionSharding(
            instruction->operand(0)->sharding(), instruction,
            may_combine_partial_sharding);
      }
      if (!IsSpatiallyPartitioned(instruction->operand(1)) &&
          !IsSpatiallyPartitioned(instruction->operand(2))) {
        return false;
      }
      if (is_spmd && IsSpatiallyPartitioned(instruction->operand(2))) {
        auto maybe_from_update =
            hlo_sharding_util::ScatterOutputShardingFromUpdate(
                instruction->operand(2)->sharding(), *instruction);
        if (maybe_from_update) {
          changed |= MaybeImproveInstructionSharding(
              std::move(*maybe_from_update), instruction,
              may_combine_partial_sharding);
        }
      }
      if (!is_spmd) {
        changed |= MaybeImproveInstructionSharding(
            HloSharding::Replicate(), instruction,
            may_combine_partial_sharding);
      }
      return changed;
    }
    case HloOpcode::kWhile: {
      if (!instruction->operand(0)->has_sharding()) {
        return false;
      }
      auto sharding = instruction->operand(0)->sharding();
      if (instruction->has_sharding()) {
        hlo_sharding_util::MergeSharding(instruction->sharding(), &sharding,
                                         may_combine_partial_sharding);
      }
      return MaybeImproveInstructionSharding(std::move(sharding), instruction,
                                             may_combine_partial_sharding);
    }
    default: {
      if (instruction->IsElementwise() && may_combine_partial_sharding) {
        bool changed = false;
        for (auto operand : instruction->operands()) {
          if (IsSpatiallyPartitioned(operand)) {
            changed |= MaybeImproveInstructionSharding(
                operand->sharding(), instruction, may_combine_partial_sharding);
          }
        }
        return changed;
      }
      const HloInstruction* operand = PickRepresentativeOperand(instruction);
      if (!operand || !IsSpatiallyPartitioned(operand)) {
        return false;
      }
      return MaybeImproveInstructionSharding(operand->sharding(), instruction,
                                             may_combine_partial_sharding);
    }
  }
  return false;
}

HloSharding InferDotOperandSharding(
    const HloInstruction* instruction,
    const dot_as_convolution_util::DotConvolutionDimsInfo& dnums,
    int64_t operand_index, bool may_combine_partial_sharding) {
  auto operand = instruction->operand(operand_index);
  auto other = instruction->operand(1 - operand_index);
  std::vector<int64_t> output_dims_to_replicate;
  std::vector<int64_t> other_operand_dims_to_replicate;
  for (const auto& dim : operand_index == 0 ? dnums.rhs_non_contracting_dims
                                            : dnums.lhs_non_contracting_dims) {
    output_dims_to_replicate.push_back(dim.output);
    other_operand_dims_to_replicate.push_back(operand_index == 0 ? dim.rhs
                                                                 : dim.lhs);
  }
  // If this dot is interpreted from a conv, then contracting dims may have
  // corresponding spatial dimensions in the output, and this operand's
  // non-contracting dims may have corresponding spatial dims in the other
  // operand.
  for (const auto& dim : dnums.contracting_dims) {
    if (dim.output >= 0) {
      output_dims_to_replicate.push_back(dim.output);
    }
  }
  for (const auto& dim : operand_index == 0 ? dnums.lhs_non_contracting_dims
                                            : dnums.rhs_non_contracting_dims) {
    int64_t other_dim = operand_index == 0 ? dim.rhs : dim.lhs;
    if (other_dim >= 0) {
      other_operand_dims_to_replicate.push_back(other_dim);
    }
  }
  auto output_other_dims_replicated =
      hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
          instruction->sharding(), output_dims_to_replicate);
  std::vector<int64_t> output_to_operand_dims(instruction->shape().rank(), -1);
  std::vector<int64_t> operand_to_output_dims(operand->shape().rank(), -1);
  for (const auto& dim : dnums.batch_dims) {
    output_to_operand_dims[dim.output] = operand_index == 0 ? dim.lhs : dim.rhs;
    operand_to_output_dims[operand_index == 0 ? dim.lhs : dim.rhs] = dim.output;
  }
  for (const auto& dim : operand_index == 0 ? dnums.lhs_non_contracting_dims
                                            : dnums.rhs_non_contracting_dims) {
    output_to_operand_dims[dim.output] = operand_index == 0 ? dim.lhs : dim.rhs;
    operand_to_output_dims[operand_index == 0 ? dim.lhs : dim.rhs] = dim.output;
  }
  auto sharding = *hlo_sharding_util::TransposeShardingWithCollapsedDims(
      output_other_dims_replicated, output_to_operand_dims,
      operand_to_output_dims);
  if (IsSpatiallyPartitioned(other)) {
    auto other_operand_dims_replicated =
        hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
            other->sharding(), other_operand_dims_to_replicate);
    std::vector<int64_t> other_to_operand_dims(other->shape().rank(), -1);
    std::vector<int64_t> operand_to_other_dims(operand->shape().rank(), -1);
    for (const auto& dim : dnums.batch_dims) {
      other_to_operand_dims[operand_index == 0 ? dim.rhs : dim.lhs] =
          operand_index == 0 ? dim.lhs : dim.rhs;
      operand_to_other_dims[operand_index == 0 ? dim.lhs : dim.rhs] =
          operand_index == 0 ? dim.rhs : dim.lhs;
    }
    for (const auto& dim : dnums.contracting_dims) {
      other_to_operand_dims[operand_index == 0 ? dim.rhs : dim.lhs] =
          operand_index == 0 ? dim.lhs : dim.rhs;
      operand_to_other_dims[operand_index == 0 ? dim.lhs : dim.rhs] =
          operand_index == 0 ? dim.rhs : dim.lhs;
    }
    HloSharding sharding_from_other =
        *hlo_sharding_util::TransposeShardingWithCollapsedDims(
            other_operand_dims_replicated, other_to_operand_dims,
            operand_to_other_dims);
    if (hlo_sharding_util::MergeSharding(sharding, &sharding_from_other,
                                         may_combine_partial_sharding)) {
      sharding = std::move(sharding_from_other);
    }
  }
  return sharding;
}

// Tries to update the sharding of the specified instruction based on its users
// and returns true if the sharding of the instruction have been changed and
// false otherwise.
bool InferShardingFromUsers(HloInstruction* instruction,
                            const ComputationMap& computation_map,
                            int64_t aggressiveness, bool is_spmd) {
  if (aggressiveness < 2 && instruction->opcode() == HloOpcode::kBroadcast) {
    return false;
  }
  // Do not change manual sharding.
  if (instruction->has_sharding() && instruction->sharding().IsManual()) {
    return false;
  }
  // Propagate manual sharding.
  if (!instruction->has_sharding() && instruction->shape().IsArray()) {
    for (const HloInstruction* user : instruction->users()) {
      if (!user->has_sharding() || !user->sharding().IsManual() ||
          user->IsCustomCall("SPMDFullToShardShape"))
        continue;
      instruction->set_sharding(
          HloSharding::Manual(user->sharding().metadata()));
      return true;
    }
  }
  if (!SupportSpatialPartitioning(instruction, computation_map, is_spmd)) {
    return false;
  }
  bool improved_sharding = false;
  const bool may_combine_partial_sharding = is_spmd && aggressiveness > 0;
  for (const HloInstruction* user : instruction->users()) {
    absl::optional<HloSharding> user_sharding =
        ShardingPropagation::GetShardingFromUser(*instruction, *user,
                                                 aggressiveness, is_spmd);
    if (user_sharding) {
      improved_sharding |= MaybeImproveInstructionSharding(
          std::move(*user_sharding), instruction, may_combine_partial_sharding);
    }
  }
  return improved_sharding;
}

// Checks if two HloShardings have the same metadata attached.
bool SameShardingMetadata(const HloSharding& a, const HloSharding& b) {
  DCHECK_EQ(a, b);

  auto same_metadata = [](absl::Span<const OpMetadata> a,
                          absl::Span<const OpMetadata> b) {
    if (a.size() != b.size()) return false;
    for (int i = 0, e = a.size(); i < e; ++i) {
      if (!protobuf_util::ProtobufEquals(a[i], b[i])) {
        return false;
      }
    }
    return true;
  };

  if (a.IsTuple()) {
    for (int i = 0, e = a.tuple_elements().size(); i < e; ++i) {
      if (!same_metadata(a.tuple_elements()[i].metadata(),
                         b.tuple_elements()[i].metadata())) {
        return false;
      }
    }
    return true;
  } else {
    return same_metadata(a.metadata(), b.metadata());
  }
}

// Assigns metadata to optional sharding on instructions if instructions have
// metadata. If sharding already has some metadata, no new metadata will be
// added.
bool AssignShardingMetadata(HloModule* module) {
  bool changed = false;
  for (HloComputation* computation : module->computations()) {
    for (HloInstruction* instruction : computation->instructions()) {
      const auto& metadata = instruction->metadata();
      if (!instruction->has_sharding() || metadata.ByteSizeLong() == 0) {
        continue;
      }

      HloSharding sharding_with_metadata =
          instruction->sharding().WithMetadata({metadata}, /*overwrite=*/false);
      if (!SameShardingMetadata(instruction->sharding(),
                                sharding_with_metadata)) {
        instruction->set_sharding(std::move(sharding_with_metadata));
        changed = true;
      }
    }
  }
  return changed;
}

// Removes all sharding metadata from shardings on instructions.
bool RemoveShardingMetadata(HloModule* module) {
  bool changed = false;
  for (HloComputation* computation : module->computations()) {
    for (HloInstruction* instruction : computation->instructions()) {
      if (!instruction->has_sharding()) {
        continue;
      }
      HloSharding sharding_no_metadata =
          instruction->sharding().WithoutMetadata();
      if (!SameShardingMetadata(instruction->sharding(),
                                sharding_no_metadata)) {
        instruction->set_sharding(std::move(sharding_no_metadata));
        changed = true;
      }
    }
  }
  return changed;
}

// Remove Sharding custom-call instruction by folding the sharding attribute
// to its operand. If the operand already has a different sharding, insert a
// copy node for reshard.
StatusOr<bool> ProcessShardingInstruction(HloModule* module) {
  bool changed = false;

  for (HloComputation* computation : module->computations()) {
    auto instructions = computation->MakeInstructionPostOrder();
    std::reverse(instructions.begin(), instructions.end());
    for (HloInstruction* instruction : instructions) {
      if (!instruction->IsCustomCall("Sharding")) {
        continue;
      }
      TF_RET_CHECK(instruction->has_sharding())
          << "Sharding instruction must have a sharding attribute";
      const HloSharding& sharding = instruction->sharding();

      // If the operand has a different sharding from the current sharding
      // instruction, create a copy node. Otherwise, just remove the sharding
      // instruction and set the operand sharding.
      if (instruction->operand(0)->has_sharding() &&
          instruction->operand(0)->sharding() != sharding) {
        auto copy = computation->AddInstruction(
            HloInstruction::CreateUnary(instruction->shape(), HloOpcode::kCopy,
                                        instruction->mutable_operand(0)));
        TF_RETURN_IF_ERROR(computation->ReplaceInstruction(instruction, copy));
        copy->set_sharding(sharding);
      } else {
        instruction->mutable_operand(0)->set_sharding(sharding);
        TF_RETURN_IF_ERROR(
            instruction->ReplaceAllUsesWith(instruction->mutable_operand(0)));
        TF_RETURN_IF_ERROR(computation->RemoveInstruction(instruction));
      }
      changed = true;
    }
  }
  return changed;
}

// If a while contains a channel instruction on device D, check that any other
// instructions with a device assignment are on D. Further, annotate the root
// instruction of the while body to ensure that HLO partitioning will keep the
// entire while instruction on D.
Status CheckAndUpdateDeviceAssignmentsInWhileBody(
    HloInstruction* while_instruction) {
  auto bad_status = [](HloInstruction* instruction, int64_t device,
                       HloInstruction* channel_instruction,
                       int64_t correct_device) {
    return FailedPrecondition(
        "Instruction: %s is on device: %d, which conflicts with device: %d "
        "of channel instruction: %s",
        instruction->name(), device, correct_device,
        channel_instruction->name());
  };

  CHECK_EQ(while_instruction->opcode(), HloOpcode::kWhile);
  HloComputation* while_body = while_instruction->while_body();
  // Maps a device number to an instruction in the while_body with that
  // device assignment.
  std::map<int64_t, HloInstruction*> devices_to_instructions;
  absl::optional<int64_t> unique_device = absl::nullopt;
  HloInstruction* channel_instruction = nullptr;

  for (HloInstruction* instruction : while_body->instructions()) {
    if (instruction->sharding_unique_device()) {
      auto opcode = instruction->opcode();
      int64_t device = *instruction->sharding_unique_device();
      if (unique_device.has_value()) {
        if (*unique_device != device) {
          return bad_status(instruction, device, channel_instruction,
                            *unique_device);
        }
      } else if (opcode == HloOpcode::kSend || opcode == HloOpcode::kRecv ||
                 // Cross-replica AllReduces don't have a channel_id, and we
                 // don't enforce any invariant about their device assignment.
                 ((opcode == HloOpcode::kAllReduce ||
                   opcode == HloOpcode::kReduceScatter) &&
                  instruction->channel_id())) {
        channel_instruction = instruction;
        unique_device = device;
        if (!devices_to_instructions.empty()) {
          for (auto it = devices_to_instructions.begin();
               it != devices_to_instructions.end(); ++it) {
            if (*unique_device != it->first) {
              return bad_status(it->second, it->first, channel_instruction,
                                *unique_device);
            }
          }
        }
      } else {
        devices_to_instructions[device] = instruction;
      }
    }
  }

  if (unique_device.has_value()) {
    auto while_device = while_instruction->sharding_unique_device();
    if (while_device.has_value() && *unique_device != *while_device) {
      return bad_status(while_instruction, *while_device, channel_instruction,
                        *unique_device);
    }
    auto body_root = while_body->root_instruction();
    auto root_device = body_root->sharding_unique_device();
    if (!root_device.has_value()) {
      body_root->set_device_sharding(*unique_device);
    } else if (*unique_device != *root_device) {
      return bad_status(body_root, *root_device, channel_instruction,
                        *unique_device);
    }
  }
  return Status::OK();
}

}  // namespace

/*static*/ Status ShardingPropagation::NormalizeDomain(
    const DomainMetadata::Domain& domain, const DomainMetadata* metadata) {
  if (metadata != nullptr) {
    TF_ASSIGN_OR_RETURN(const auto& sharding_metadata,
                        ShardingMetadata::ToShardingMetadata(metadata));
    const auto& sharding = sharding_metadata->sharding();
    if (sharding != nullptr) {
      bool is_spatially_partitioned = !sharding->HasUniqueDevice();
      if (sharding->IsTuple()) {
        is_spatially_partitioned = absl::c_any_of(
            sharding->tuple_elements(),
            [](const HloSharding& s) { return !s.HasUniqueDevice(); });
      }
      if (is_spatially_partitioned) {
        for (HloInstruction* d : domain.exit_domains) {
          d->mutable_operand(0)->set_sharding(*sharding);
        }
        return Status::OK();
      }
    }
  }
  return ShardingMetadata::NormalizeShardingDomain(domain, metadata);
}

// Return the sharding that should be propagated from user to instruction.
absl::optional<HloSharding> ShardingPropagation::GetShardingFromUser(
    const HloInstruction& instruction, const HloInstruction& user,
    int64_t aggressiveness, bool is_spmd) {
  if (!CanPropagateThroughAtAgressiveLevel(user, aggressiveness)) {
    return absl::nullopt;
  }
  if (!IsSpatiallyPartitioned(&user)) {
    return absl::nullopt;
  }
  const bool may_combine_partial_sharding = is_spmd && aggressiveness > 0;

  switch (user.opcode()) {
    case HloOpcode::kBroadcast: {
      if (user.sharding().IsReplicated()) {
        return user.sharding();
      }
      std::vector<int64_t> dims_to_replicate;
      bool needs_replication = false;
      for (int64_t i = 0; i < user.shape().rank(); ++i) {
        if (absl::c_count(user.dimensions(), i) == 0) {
          dims_to_replicate.push_back(i);
          if (user.sharding().tile_assignment().dim(i) > 1) {
            needs_replication = true;
          }
        }
      }
      // If not SPMD, only support when none of the partitioned dimensions in
      // the broadcast output belong to new dimensions.
      if (!is_spmd && needs_replication) {
        return absl::nullopt;
      }
      return hlo_sharding_util::RemoveShapeDimensions(
          hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
              user.sharding(), dims_to_replicate),
          dims_to_replicate);
    }
    case HloOpcode::kConcatenate: {
      if (user.sharding().IsReplicated()) {
        return user.sharding();
      }

      const int64_t cdim = user.concatenate_dimension();
      const Array<int64_t>& tile_assignment = user.sharding().tile_assignment();
      if (tile_assignment.dim(cdim) == 1) {
        // If we are concatenating along a non-sharded dimension then the
        // operands should have the same sharding as the result.
        return user.sharding();
      }

      if (is_spmd) {
        // SPMD doesn't support tiling with part of the devices. Return the same
        // sharding.
        return user.sharding();
      }

      // If we are concatenating along a sharded dimension then we want the
      // operands to be distributed among the devices their data is used.
      int64_t start_offset = 0;
      for (HloInstruction* op : user.operands()) {
        if (op == &instruction) {
          break;
        }
        start_offset += op->shape().dimensions(cdim);
      }
      const int64_t tile_shape = CeilOfRatio(
          user.shape().dimensions(cdim), tile_assignment.dimensions()[cdim]);
      std::vector<int64_t> start_indices(tile_assignment.num_dimensions());
      std::vector<int64_t> end_indices = tile_assignment.dimensions();
      start_indices[cdim] = start_offset / tile_shape;
      end_indices[cdim] = CeilOfRatio(
          start_offset + instruction.shape().dimensions(cdim), tile_shape);
      auto new_tile_assignment =
          tile_assignment.Slice(start_indices, end_indices);
      if (new_tile_assignment.num_elements() == 1) {
        return HloSharding::AssignDevice(*new_tile_assignment.begin(),
                                         user.sharding().metadata());
      }
      return HloSharding::Tile(new_tile_assignment, user.sharding().metadata());
    }
    case HloOpcode::kConvolution: {
      auto dot_dims = dot_as_convolution_util::ParseConvolutionDimsInfo(&user);
      if (dot_dims.conv_spatial_dims.empty()) {
        int64_t op_idx = user.operand_index(&instruction);
        return InferDotOperandSharding(&user, dot_dims, op_idx,
                                       may_combine_partial_sharding);
      }
      return absl::nullopt;
    }
    case HloOpcode::kDynamicSlice:
    case HloOpcode::kDynamicUpdateSlice: {
      if (user.sharding().IsReplicated()) {
        return user.sharding();
      }
      if (user.opcode() == HloOpcode::kDynamicUpdateSlice &&
          &instruction == user.operand(0)) {
        return user.sharding();
      }
      const HloInstruction* operand = user.opcode() == HloOpcode::kDynamicSlice
                                          ? user.operand(0)
                                          : user.operand(1);
      if (&instruction != operand) {
        return absl::nullopt;
      }

      if (is_spmd) {
        return user.sharding();
      }
      const auto& tile_assignment = user.sharding().tile_assignment();
      for (int64_t i = 0; i < user.shape().rank(); ++i) {
        if (tile_assignment.dim(i) > 1 &&
            user.shape().dimensions(i) != operand->shape().dimensions(i)) {
          return absl::nullopt;
        }
      }
      return user.sharding();
    }
    case HloOpcode::kReduceWindow: {
      auto* reduce_window = Cast<HloReduceWindowInstruction>(&user);
      if (!absl::c_linear_search(reduce_window->inputs(), &instruction)) {
        return absl::nullopt;
      }
      if (reduce_window->shape().IsTuple()) {
        auto sub_sharding = reduce_window->sharding().GetSubSharding(
            reduce_window->shape(),
            {reduce_window->operand_index(&instruction)});
        return sub_sharding;
      }
      return reduce_window->sharding();
    }
    case HloOpcode::kReshape: {
      return hlo_sharding_util::ReshapeSharding(
          user.shape(), instruction.shape(), user.sharding());
    }
    case HloOpcode::kPad: {
      if (&instruction != user.operand(0)) {
        return absl::nullopt;
      }
      return user.sharding();
    }
    case HloOpcode::kSlice: {
      return user.sharding();
    }
    case HloOpcode::kTranspose: {
      // Calculate the dimension numbers for reversing the current transpose
      // and then use TransposeSharding to convert the output sharding to an
      // input sharding.
      std::vector<int64_t> reverse_dimensions(user.dimensions().size());
      for (int64_t i = 0; i < user.dimensions().size(); ++i) {
        reverse_dimensions[user.dimensions(i)] = i;
      }
      return hlo_sharding_util::TransposeSharding(user.sharding(),
                                                  reverse_dimensions);
    }
    case HloOpcode::kTuple: {
      auto sub_sharding = user.sharding().GetSubSharding(
          user.shape(), {user.operand_index(&instruction)});
      return sub_sharding;
    }
    case HloOpcode::kGetTupleElement: {
      int64_t sharding_index = 0;
      for (int64_t i = 0; i < instruction.shape().tuple_shapes_size(); ++i) {
        if (i == user.tuple_index()) {
          break;
        }
        if (instruction.shape().tuple_shapes(i).IsArray()) {
          sharding_index += 1;
        } else {
          sharding_index +=
              instruction.shape().tuple_shapes(i).tuple_shapes_size();
        }
      }
      if (user.shape().IsArray()) {
        // Use ReplicateAllDataDims instead of HloSharding::Replicate() to
        // preserve manual subgroups.
        HloSharding new_sharding =
            instruction.has_sharding()
                ? instruction.sharding()
                : HloSharding::SingleTuple(
                      instruction.shape(),
                      hlo_sharding_util::ReplicateAllDataDims(user.sharding()));
        new_sharding.tuple_elements()[sharding_index] = user.sharding();
        return new_sharding;
      } else {
        if (user.sharding().tuple_elements().empty()) {
          return absl::nullopt;
        }
        HloSharding new_sharding =
            instruction.has_sharding()
                ? instruction.sharding()
                : HloSharding::SingleTuple(
                      instruction.shape(),
                      hlo_sharding_util::ReplicateAllDataDims(
                          user.sharding().tuple_elements()[0]));
        for (int64_t i = 0; i < user.sharding().tuple_elements().size(); ++i) {
          new_sharding.tuple_elements()[sharding_index + i] =
              user.sharding().tuple_elements()[i];
        }
        return new_sharding;
      }
    }
    case HloOpcode::kDot: {
      int64_t op_idx = user.operand_index(&instruction);
      auto dnums = dot_as_convolution_util::ParseDotGeneralFromDot(&user);
      return InferDotOperandSharding(&user, dnums, op_idx,
                                     may_combine_partial_sharding);
    }
    case HloOpcode::kReduce: {
      if (instruction.shape().rank() == 0) {
        return absl::nullopt;
      }
      auto user_sharding =
          user.shape().IsTuple()
              ? user.sharding().GetSubSharding(
                    user.shape(), {user.operand_index(&instruction)})
              : user.sharding();
      if (user_sharding.IsTileMaximal()) {
        return user_sharding;
      }
      std::vector<int64_t> target_tile_assignment_dimensions(
          instruction.shape().rank() +
          (user_sharding.ReplicateOnLastTileDim() ? 1 : 0) +
          user_sharding.subgroup_types().size());
      const auto& dimensions = user.dimensions();
      int64_t next_output_dim = 0;
      for (int64_t i = 0; i < target_tile_assignment_dimensions.size(); ++i) {
        if (absl::c_find(dimensions, i) == dimensions.end()) {
          target_tile_assignment_dimensions[i] =
              user_sharding.tile_assignment().dim(next_output_dim++);
        } else {
          target_tile_assignment_dimensions[i] = 1;
        }
      }
      auto tile_assignment = user_sharding.tile_assignment();
      tile_assignment.Reshape(target_tile_assignment_dimensions);
      return user_sharding.ReplicateOnLastTileDim()
                 ? HloSharding::PartialTile(tile_assignment,
                                            user_sharding.metadata())
                 : HloSharding::Subgroup(tile_assignment,
                                         user_sharding.subgroup_types(),
                                         user_sharding.metadata());
    }
    case HloOpcode::kSort: {
      HloSharding user_sharding = user.sharding();
      if (user_sharding.IsTuple()) {
        return user_sharding = user_sharding.GetSubSharding(
                   user.shape(), {user.operand_index(&instruction)});
      }
      return user_sharding;
    }
    case HloOpcode::kReverse: {
      return hlo_sharding_util::ReverseSharding(user.sharding(),
                                                user.dimensions());
    }
    case HloOpcode::kGather: {
      if (&instruction == user.operand(1)) {
        return hlo_sharding_util::GatherIndexSharding(user.sharding(), &user);
      }
      if (is_spmd) {
        return hlo_sharding_util::GatherDataOperandShardingFromOutput(
            user.sharding(), user);
      }
      return absl::nullopt;
    }
    case HloOpcode::kScatter: {
      if (&instruction == user.operand(0)) {
        return user.sharding();
      }
      if (&instruction == user.operand(1)) {
        auto update = user.operand(2);
        if (!IsSpatiallyPartitioned(update)) {
          return absl::nullopt;
        }
        return hlo_sharding_util::ScatterIndexSharding(update->sharding(),
                                                       &user);
      }
      CHECK_EQ(&instruction, user.operand(2));
      auto indices = user.operand(1);
      if (IsSpatiallyPartitioned(indices)) {
        auto from_indices =
            hlo_sharding_util::ScatterDataSharding(indices->sharding(), &user);
        if (!from_indices.IsTileMaximal()) {
          return from_indices;
        }
      }
      if (is_spmd) {
        return hlo_sharding_util::ScatterUpdateShardingFromOutput(
            user.sharding(), user);
      }
      return absl::nullopt;
    }
    default: {
      // If the user output shape is compatible with the current instruction
      // shape excluding element type and the current instruction is supported
      // by spatial partitioning, then the user sharding can be used for
      // propagation to the current instruction.
      if (ShapeUtil::CompatibleIgnoringElementType(instruction.shape(),
                                                   user.shape())) {
        return user.sharding();
      }
      return absl::nullopt;
    }
  }
}

StatusOr<bool> ShardingPropagation::Run(HloModule* module) {
  bool any_changed = propagate_metadata_ ? AssignShardingMetadata(module)
                                         : RemoveShardingMetadata(module);

  auto status_or_changed = ProcessShardingInstruction(module);
  if (!status_or_changed.ok()) return status_or_changed;
  any_changed |= status_or_changed.ValueOrDie();

  // Association of partitionable embedded computations with their parent
  // instruction.
  ComputationMap computation_map;

  // Instructions that are related through a computation and need to share the
  // same sharding.
  auto get_related_instructions = [](HloInstruction* inst) {
    if (inst->opcode() == HloOpcode::kWhile) {
      return std::vector<HloInstruction*>{
          inst, inst->while_body()->root_instruction(),
          inst->while_body()->parameter_instruction(0),
          inst->while_condition()->parameter_instruction(0)};
    } else if (inst->opcode() == HloOpcode::kConditional) {
      std::vector<HloInstruction*> comps{inst};
      for (HloComputation* c : inst->called_computations()) {
        comps.push_back(c->root_instruction());
      }
      return comps;
    } else {
      CHECK(false);
    }
  };

  // If instruction is a while, or the root or a parameter of a while body,
  // then propagate its sharding to the while instruction, to its body root,
  // and to its condition parameter.
  std::function<void(HloInstruction*, absl::flat_hash_set<HloInstruction*>*)>
      maybe_computation_propagation =
          [&](HloInstruction* instruction,
              absl::flat_hash_set<HloInstruction*>* changed) {
            auto propagate_to_instruction = [&](HloInstruction* search_inst) {
              auto related_instructions = get_related_instructions(search_inst);
              if (absl::c_count(related_instructions, instruction)) {
                for (HloInstruction* inst : related_instructions) {
                  if (!inst->has_sharding() ||
                      inst->sharding() != instruction->sharding()) {
                    VLOG(2) << "Add computation sharding: " << inst->name();
                    inst->set_sharding(instruction->sharding());
                    changed->insert(inst);
                    maybe_computation_propagation(inst, changed);
                  }
                }
              }
            };

            if (instruction->opcode() == HloOpcode::kConditional ||
                instruction->opcode() == HloOpcode::kWhile) {
              propagate_to_instruction(instruction);
            }

            if (instruction->opcode() == HloOpcode::kParameter ||
                instruction->parent()->root_instruction() == instruction) {
              auto it = computation_map.find(instruction->parent());
              if (it != computation_map.end()) {
                propagate_to_instruction(it->second);
              }
            }
          };

  for (auto computation : module->computations()) {
    for (auto instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kWhile) {
        TF_RETURN_IF_ERROR(
            CheckAndUpdateDeviceAssignmentsInWhileBody(instruction));
      }
    }
  }

  // Populate computation_map in order to associate while bodies to their
  // while instructions.
  for (auto computation : module->computations()) {
    for (auto instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kWhile ||
          instruction->opcode() == HloOpcode::kConditional) {
        // Check if any of the related instructions has sharding, in which case
        // propagate it to the other instructions, so they all share the same
        // sharding, in case the user didn't shard all of them. We don't check
        // that user shardings are consistent, because such check is already
        // done by HloShardingVerifier.
        const HloInstruction* sharded_inst = nullptr;
        auto related_instructions = get_related_instructions(instruction);
        for (auto inst : related_instructions) {
          if (inst->has_sharding()) {
            sharded_inst = inst;
            break;
          }
        }
        if (sharded_inst != nullptr) {
          // Set the same sharding to all the other related instructions.
          for (auto inst : related_instructions) {
            inst->set_sharding(sharded_inst->sharding());
          }
        }
        if (instruction->opcode() == HloOpcode::kWhile) {
          computation_map[instruction->while_body()] = instruction;
        } else {
          for (HloComputation* c : instruction->called_computations()) {
            computation_map[c] = instruction;
          }
        }
      }
    }
  }

  // Collect all pre-sharded instructions as we aren't allowed to modify their
  // sharding.
  absl::flat_hash_set<const HloInstruction*> provided_shardings;
  for (const HloComputation* computation : module->computations()) {
    for (const HloInstruction* inst : computation->instructions()) {
      if (inst->has_sharding()) {
        provided_shardings.insert(inst);
      }
    }
  }

  // Consider the root instruction of the entry module as one with provided
  // sharding as its sharding have to match with the one expected by the host.
  provided_shardings.insert(module->entry_computation()->root_instruction());

  // Iterate to a fixpoint that is guaranteed to be reached because we only
  // strictly improve the sharding of the graph and it can't be improved
  // indefinitely.
  int64_t iterations = 0;
  auto run_to_fix_point = [&](int64_t aggressiveness) {
    absl::flat_hash_set<const HloInstruction*> already_inferred_from_operands;
    absl::flat_hash_set<const HloInstruction*> already_inferred_from_users;
    bool changed_last_iter = true;
    while (changed_last_iter) {
      changed_last_iter = false;
      int64_t inferred_from_operand_counter = 0;
      int64_t inferred_from_user_counter = 0;
      int64_t instruction_counter = 0;
      int64_t already_sharded_counter = 0;
      for (const HloComputation* computation : module->computations()) {
        std::vector<HloInstruction*> instructions =
            computation->MakeInstructionPostOrder();

        instruction_counter += instructions.size();
        for (const HloInstruction* instruction : instructions) {
          already_sharded_counter += (instruction->has_sharding() ? 1 : 0);
        }
        auto clear_cache = [&](HloInstruction* hlo) {
          for (auto operand : hlo->operands()) {
            already_inferred_from_users.erase(operand);
          }
          for (auto user : hlo->users()) {
            already_inferred_from_operands.erase(user);
          }
        };
        // First iterate the HLO graph in post order taking shardings from
        // operands.
        for (HloInstruction* instruction : instructions) {
          if (already_inferred_from_operands.contains(instruction) ||
              provided_shardings.contains(instruction)) {
            continue;
          }
          already_inferred_from_operands.insert(instruction);
          if (InferShardingFromOperands(instruction, computation_map, is_spmd_,
                                        aggressiveness)) {
            ++inferred_from_operand_counter;
            any_changed = true;
            VLOG(2) << "Add sharding (forward-pass): "
                    << instruction->ToString();
            absl::flat_hash_set<HloInstruction*> changed_in_comp_prop;
            maybe_computation_propagation(instruction, &changed_in_comp_prop);
            clear_cache(instruction);
            for (auto hlo : changed_in_comp_prop) {
              clear_cache(hlo);
            }
            changed_last_iter = true;
          }
        }

        // Then iterate the HLO graph in reverse post order taking shardings
        // from users.
        for (auto it = instructions.rbegin(); it != instructions.rend(); ++it) {
          if (already_inferred_from_users.contains(*it) ||
              provided_shardings.contains(*it)) {
            continue;
          }
          already_inferred_from_users.insert(*it);
          if (InferShardingFromUsers(*it, computation_map, aggressiveness,
                                     is_spmd_)) {
            ++inferred_from_user_counter;
            any_changed = true;
            VLOG(2) << "Add sharding (backward-pass): " << (*it)->ToString();
            absl::flat_hash_set<HloInstruction*> changed_in_comp_prop;
            maybe_computation_propagation(*it, &changed_in_comp_prop);
            clear_cache(*it);
            for (auto hlo : changed_in_comp_prop) {
              clear_cache(hlo);
            }
            changed_last_iter = true;
          }
        }
      }
      VLOG(1) << "Sharding propagation iteration " << iterations << ";";
      VLOG(1) << "  total instructions: " << instruction_counter;
      VLOG(1) << "  instructions already sharded: " << already_sharded_counter;
      VLOG(1) << "  shardings inferred from operands: "
              << inferred_from_operand_counter;
      VLOG(1) << "  shardings inferred from users: "
              << inferred_from_user_counter;
      VLOG(1) << "  aggressiveness: " << aggressiveness;
      ++iterations;
    }
    return Status::OK();
  };
  for (int64_t aggressiveness = 0; aggressiveness < 4; ++aggressiveness) {
    TF_RETURN_IF_ERROR(run_to_fix_point(aggressiveness));
  }

  VLOG(1) << "Sharding propagation completed after " << iterations
          << " iterations";
  return any_changed;
}

}  // namespace xla
