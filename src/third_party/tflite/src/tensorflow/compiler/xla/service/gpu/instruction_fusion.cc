/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/xla/service/gpu/instruction_fusion.h"

#include "absl/container/flat_hash_set.h"
#include "tensorflow/compiler/xla/service/fusion_node_indexing_evaluation.h"
#include "tensorflow/compiler/xla/service/gpu/gpu_fusible.h"
#include "tensorflow/compiler/xla/service/gpu/ir_emission_utils.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/hlo_query.h"
#include "tensorflow/compiler/xla/service/llvm_ir/fused_ir_emitter.h"
#include "tensorflow/compiler/xla/service/pattern_matcher.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

namespace xla {
namespace gpu {

namespace {
bool ElementIsF32OrF16(const Shape& shape) {
  PrimitiveType type = shape.element_type();
  return type == F32 || type == F16;
}
}  // namespace

/*static*/ bool GpuInstructionFusion::IsExpensive(
    const HloInstruction& instruction) {
  // We say that some floating-point math ops are cheap on the GPU. Unlike other
  // intrinsics that can be expanded into many instructions, Div and Rsqrt are
  // lowered into single hardware instructions.
  switch (instruction.opcode()) {
    case HloOpcode::kDivide:
    case HloOpcode::kRsqrt:
      if (ElementIsF32OrF16(instruction.shape())) {
        return false;
      }
      break;
    default:
      break;
  }
  return InstructionFusion::IsExpensive(instruction);
}

bool GpuInstructionFusion::ShouldFuseInexpensiveChecks(HloInstruction* consumer,
                                                       int64_t operand_index) {
  HloInstruction* producer = consumer->mutable_operand(operand_index);

  // Output fusions are not currently supported on GPUs.
  if (producer->opcode() == HloOpcode::kFusion) {
    VLOG(4) << "Producer " << producer->name() << " is a fusion op";
    return false;
  }
  // Cost condition: not fuse (simple, expensive producers) and (consumers who
  // reuse operand elements).
  if (producer->opcode() != HloOpcode::kFusion && is_expensive(*producer) &&
      ReusesOperandElements(consumer, operand_index)) {
    VLOG(4) << "Do not fuse simple, expensive producer " << producer->name()
            << " and consumer which reuses operand elements.";
    return false;
  }

  if (!IsProducerConsumerFusible(*producer, *consumer) ||
      !InstructionFusion::ShouldFuse(consumer, operand_index)) {
    VLOG(4) << "Producer " << producer->name()
            << " is not fusible or should not be fused.";
    return false;
  }
  return true;
}

bool GpuInstructionFusion::ShouldFuse(HloInstruction* consumer,
                                      int64_t operand_index) {
  if (!ShouldFuseInexpensiveChecks(consumer, operand_index)) {
    VLOG(5) << "Not fusing inexpensive checks of operand " << operand_index
            << " of " << consumer->ToString();
    return false;
  }
  auto producer = consumer->operand(operand_index);

  // The following checks are potentially expensive.
  if (FusionWouldBeTooLarge(*consumer, *producer,
                            /*is_consumer_producer_fusion=*/true)) {
    VLOG(5) << "Fusion of (" << producer->ToString() << ") into ("
            << consumer->ToString() << ") would be too large";
    return false;
  }
  if (consumer->opcode() != HloOpcode::kFusion) {
    return true;
  }
  // Also check that our emitter can handle the fusion node. We currently can
  // have exponential time/memory requirements for emitting certain fusion
  // kernels, in which case we don't want to fuse.
  // TODO(b/119692968): Remove this once we have fixed our fusion emitter.
  if (fusion_node_evaluations_.find(consumer) ==
      fusion_node_evaluations_.end()) {
    // We have no cached results for this fusion node yet. This can happen when
    // we run the InstructionFusion pass more than once. We can only cache the
    // results within one run.
    fusion_node_evaluations_.emplace(consumer,
                                     FusionNodeIndexingEvaluation(consumer));
  }
  if (fusion_node_evaluations_.at(consumer).CodeDuplicationTooHigh(producer)) {
    VLOG(5) << "Fusion of " << producer->name() << " into " << consumer->name()
            << " would result in overly large code duplication.";
    return false;
  }
  return true;
}

bool GpuInstructionFusion::ShouldFuseIntoMultiOutput(HloInstruction* consumer,
                                                     int64_t operand_index) {
  return false;
}

HloInstruction::FusionKind GpuInstructionFusion::ChooseKind(
    const HloInstruction* producer, const HloInstruction* consumer) {
  return ChooseFusionKind(*producer, *consumer);
}

HloInstruction* GpuInstructionFusion::FuseInstruction(
    HloInstruction* fusion_instruction, HloInstruction* producer) {
  auto evaluation = fusion_node_evaluations_.find(fusion_instruction);
  if (evaluation == fusion_node_evaluations_.end()) {
    evaluation = fusion_node_evaluations_
                     .emplace(fusion_instruction,
                              FusionNodeIndexingEvaluation(fusion_instruction))
                     .first;
  }
  auto indexing_users = evaluation->second.RemoveFusionOperand(producer);
  HloInstruction* new_producer =
      InstructionFusion::FuseInstruction(fusion_instruction, producer);
  evaluation->second.UpdateEvaluationCache(new_producer, indexing_users);
  return new_producer;
}

}  // namespace gpu
}  // namespace xla
