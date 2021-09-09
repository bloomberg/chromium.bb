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

#include "tensorflow/compiler/xla/service/fusion_node_indexing_evaluation.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/core/platform/logging.h"

namespace xla {

FusionNodeIndexingEvaluation::FusionNodeIndexingEvaluation(
    const HloInstruction* fusion)
    : fusion_(fusion) {
  total_emitted_instructions_ = 0;
  HloInstruction* root = fusion->fused_expression_root();
  indexing_users_[root].insert(fusion);
  index_usage_count_[fusion] = 1;
  RecomputeCache();
}

bool FusionNodeIndexingEvaluation::AverageCodeDuplicationTooHigh(
    const HloInstruction* producer) const {
  // This constant is arbitrarily chosen. Essentially we don't want to have too
  // much code duplication, because it slows down the compilation time. There is
  // a tradeoff between compilation time and runtime here.
  const int64 kAllowedCodeDuplication = 15;

  // index_usage_count_ contains an entry for each instruction in the fusion
  // computation (except parameter instructions), plus an entry for the 'fusion'
  // instruction. So the size of this map is already one bigger than the number
  // of instructions in the fusion node that are emitted, thus accounting for
  // the number of instructions after 'producer' is fused.
  return EvaluateTotalEmittedInstructions(producer) /
             index_usage_count_.size() >
         kAllowedCodeDuplication;
}

int64 FusionNodeIndexingEvaluation::EvaluateTotalEmittedInstructions(
    const HloInstruction* producer) const {
  int64 total = total_emitted_instructions_;
  for (const auto* user : indexing_users_.at(producer)) {
    total += index_usage_count_.at(user);
  }
  return total;
}

void FusionNodeIndexingEvaluation::UpdateEvaluationCache(
    const HloInstruction* producer,
    absl::flat_hash_set<const HloInstruction*> indexing_users_of_producer) {
  CHECK(!indexing_users_.contains(producer));
  indexing_users_[producer] = std::move(indexing_users_of_producer);
  UpdateIndexUsageCount(producer);
  UpdateIndexingUsersOfOperands(producer);
}

absl::flat_hash_set<const HloInstruction*>
FusionNodeIndexingEvaluation::RemoveFusionOperand(
    HloInstruction* fusion_operand) {
  auto indexing_users_of_operand =
      std::move(indexing_users_.at(fusion_operand));
  indexing_users_.erase(fusion_operand);
  CHECK(!index_usage_count_.contains(fusion_operand));
  return indexing_users_of_operand;
}

void FusionNodeIndexingEvaluation::RecomputeCache() {
  auto postorder =
      fusion_->fused_instructions_computation()->MakeInstructionPostOrder();
  std::reverse(postorder.begin(), postorder.end());
  for (const auto* instruction : postorder) {
    if (instruction->opcode() == HloOpcode::kParameter) {
      continue;
    }
    UpdateIndexUsageCount(instruction);
    UpdateIndexingUsersOfOperands(instruction);
  }
}

void FusionNodeIndexingEvaluation::UpdateIndexUsageCount(
    const HloInstruction* instruction) {
  int64 total = 0;
  for (const auto* user : indexing_users_[instruction]) {
    int64 weight = 1;
    // Concatenate is special: the index differs for each operand, so
    // in the worst case we have to deal with as many index values as
    // the number of operands of Concatenate. By considering the worst
    // case, we are more conservative than necessary regarding
    // counting the index usage.
    if (user->opcode() == HloOpcode::kConcatenate) {
      weight = user->operand_count();
    }
    total += index_usage_count_.at(user) * weight;
  }
  CHECK(index_usage_count_.emplace(instruction, total).second);
  total_emitted_instructions_ += total;
}

void FusionNodeIndexingEvaluation::UpdateIndexingUsersOfOperands(
    const HloInstruction* instruction) {
  for (const auto* operand : instruction->operands()) {
    if (operand->opcode() == HloOpcode::kParameter) {
      // Although actually the parameter gets indexed, we store it as indexing
      // of the corresponding fusion operand instead because parameter
      // instruction pointers can be invalidated when we fuse another
      // instruction into 'fusion_'.
      operand = fusion_->operand(operand->parameter_number());
    }
    // For simplicity we assume that all shape and layout changing
    // operations except Transposes invalidate index reuse. Transposes are
    // special: although they are shape changing, we can reuse the
    // multi-dimensional index for the operand by permuting it.
    if (instruction->opcode() == HloOpcode::kTranspose ||
        Shape::Equal().IgnoreElementType()(operand->shape(),
                                           instruction->shape())) {
      // If the index is reused, it means the operand gets index values
      // from the same set of (indirect) users as 'instruction' itself.
      indexing_users_[operand].insert(indexing_users_[instruction].begin(),
                                      indexing_users_[instruction].end());
    } else {
      // If the index is not reused, it means 'instruction' computes a
      // new index derived from the index it gets.
      indexing_users_[operand].insert(instruction);
    }
  }
}

}  // namespace xla
