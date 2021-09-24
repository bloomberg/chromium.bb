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

#include "tensorflow/compiler/xla/service/hlo_rematerialization.h"

#include <memory>
#include <string>

#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_matchers.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/hlo_ordering.h"
#include "tensorflow/compiler/xla/service/hlo_rematerialization_test_utils.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/tests/hlo_test_base.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/core/lib/core/status_test_util.h"

namespace xla {
namespace {

namespace op = xla::testing::opcode_matchers;

using ::testing::_;

// Inherits methods to create rematerializable computations. See
// RematerializationTestBase for more.
class HloRematerializationTest : public RematerializationTestBase {
 protected:
  StatusOr<bool> RunHloRematerialization(int64_t memory_limit_bytes,
                                         HloModule* module,
                                         int64_t min_remat_size = 0) {
    TF_EXPECT_OK(verifier().Run(module).status());
    HloMemoryScheduler scheduler(
        [](const BufferValue& buffer) { return ByteSizeOf(buffer.shape()); },
        ComputationSchedulerToModuleScheduler(DefaultMemoryScheduler));
    TF_EXPECT_OK(scheduler.Run(module).status());
    HloRematerialization remat(
        ByteSizeOf, memory_limit_bytes,
        /*sizes=*/nullptr,
        HloRematerialization::RematerializationPass::kPreFusion,
        /*block_size_limit=*/1, /*block_rematerialization_factor=*/1, nullptr,
        HloRematerialization::RematerializationMode::kRecomputeAndCompress,
        min_remat_size);
    return remat.Run(module);
  }
};

// Test rematerialization of a single computation produced by
// MakeRematerializableComputation.
TEST_F(HloRematerializationTest, SingleComputation) {
  auto module = CreateNewVerifiedModule();
  HloComputation* computation =
      module->AddEntryComputation(MakeRematerializableComputation());

  // Find and save the original broadcast instruction which should be
  // rematerialized.
  const HloInstruction* slice = computation->root_instruction();
  ASSERT_THAT(slice, op::Slice(op::Concatenate(op::Broadcast(_), _)));
  const HloInstruction* concat = slice->operand(0);
  const HloInstruction* bcast = concat->operand(0);

  // Computation requires 16KB without rematerialization, but uses only 12KB
  // with rematerialization so pick a memory limit between these values (14KB).
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/14 * 1024, module.get()));
  EXPECT_TRUE(changed);

  // Root should not have changed.
  EXPECT_EQ(computation->root_instruction(), slice);

  // The broadcast should have been rematerialized.
  const HloInstruction* remat_bcast = concat->operand(0);
  EXPECT_THAT(remat_bcast, op::Broadcast(::testing::Ne(bcast)));

  // The rematerialized broadcast should be immediate before the concat in the
  // sequence.
  EXPECT_EQ(module->schedule()
                .sequence(computation)
                .instructions()[computation->instruction_count() - 2],
            concat);
  EXPECT_EQ(module->schedule()
                .sequence(computation)
                .instructions()[computation->instruction_count() - 3],
            remat_bcast);
}

// Test rematerialization of a single computation that contains nodes that
// doesn't contain node worth using remat.
TEST_F(HloRematerializationTest, SingleComputationNoWorthRemat) {
  auto module = CreateNewVerifiedModule();
  HloComputation* computation =
      module->AddEntryComputation(MakeRematerializableComputation());

  // Find and save the original broadcast instruction which should be
  // rematerialized.
  const HloInstruction* slice = computation->root_instruction();
  ASSERT_THAT(slice, op::Slice(op::Concatenate(op::Broadcast(_), _)));

  // Set the minimum remat size to 14KiB, meaning no nodes should be remat.
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/14 * 1024, module.get(),
                              /*min_remat_size=*/14 * 1024));
  EXPECT_FALSE(changed);
}

// Test rematerialization of a single computation produced by
// MakeRematerializableComputation but with a sufficiently high memory limit
// such that no instructions are rematerialized.
TEST_F(HloRematerializationTest, SingleComputationNoRematerialization) {
  auto module = CreateNewVerifiedModule();
  HloComputation* computation =
      module->AddEntryComputation(MakeRematerializableComputation());

  EXPECT_EQ(computation->instruction_count(), 8);

  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/20 * 1024, module.get()));

  // No instructions should have been materialized.
  EXPECT_FALSE(changed);
  EXPECT_EQ(computation->instruction_count(), 8);
}

// Test rematerialization of a computation which calls another computation via a
// while. Both the entry computation and while body computation can have memory
// usage reduced via rematerialization however the memory limit is set such that
// only one computation needs to have an instruction rematerialized. The entry
// computation should be the one chosen because rematerialization in the while
// will presumably be more expensive.
TEST_F(HloRematerializationTest, RematerializeAroundWhile) {
  auto module = CreateNewVerifiedModule();

  auto cond_builder = HloComputation::Builder(TestName() + ".cond");
  cond_builder.AddInstruction(
      HloInstruction::CreateParameter(0, vec1_shape_, "param"));
  cond_builder.AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::CreateR0<bool>(true)));
  HloComputation* while_cond =
      module->AddEmbeddedComputation(cond_builder.Build());

  HloComputation* body_computation = module->AddEmbeddedComputation(
      MakeRematerializableComputation(/*suffix=*/".body"));
  HloComputation* entry_computation =
      module->AddEntryComputation(MakeRematerializableWhileComputation(
          while_cond, /*while_body=*/body_computation));

  EXPECT_EQ(entry_computation->instruction_count(), 7);
  EXPECT_EQ(body_computation->instruction_count(), 8);

  // The body computation uses 16KB and the entry computation uses 2KB at the
  // while so the peak memory use of the module is 18KB. Set the memory limit a
  // bit lower (17KB) to force rematerialization of the entry computation.
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/17 * 1024, module.get()));
  EXPECT_TRUE(changed);

  // Only the entry computation should have a rematerialized instruction added.
  EXPECT_EQ(entry_computation->instruction_count(), 8);
  EXPECT_EQ(body_computation->instruction_count(), 8);
}

// Test rematerialization of a computation which calls another computation via a
// while. Both the entry computation and while body computation should have
// computations rematerialized.
TEST_F(HloRematerializationTest, RematerializeEntryAndWhileBody) {
  auto module = CreateNewVerifiedModule();

  auto cond_builder = HloComputation::Builder(TestName() + ".cond");
  cond_builder.AddInstruction(
      HloInstruction::CreateParameter(0, vec1_shape_, "param"));
  cond_builder.AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::CreateR0<bool>(true)));
  HloComputation* while_cond =
      module->AddEmbeddedComputation(cond_builder.Build());

  HloComputation* body_computation = module->AddEmbeddedComputation(
      MakeRematerializableComputation(/*suffix=*/".body"));
  HloComputation* entry_computation =
      module->AddEntryComputation(MakeRematerializableWhileComputation(
          while_cond, /*while_body=*/body_computation));

  EXPECT_EQ(entry_computation->instruction_count(), 7);
  EXPECT_EQ(body_computation->instruction_count(), 8);

  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/15 * 1024, module.get()));
  EXPECT_TRUE(changed);

  // Both computations should have rematerialized instructions added.
  EXPECT_EQ(entry_computation->instruction_count(), 9);
  EXPECT_EQ(body_computation->instruction_count(), 9);
}

// Test rematerialization of a doubly nested computation. All computations
// should have an instruction rematerialized.
TEST_F(HloRematerializationTest, RematerializeNestedComputations) {
  auto module = CreateNewVerifiedModule();

  auto cond_builder = HloComputation::Builder(TestName() + ".cond");
  cond_builder.AddInstruction(
      HloInstruction::CreateParameter(0, vec1_shape_, "param"));
  cond_builder.AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::CreateR0<bool>(true)));
  HloComputation* while_cond =
      module->AddEmbeddedComputation(cond_builder.Build());
  HloComputation* while_cond_copy =
      module->AddEmbeddedComputation(while_cond->Clone());

  HloComputation* inner_computation = module->AddEmbeddedComputation(
      MakeRematerializableComputation(/*suffix=*/".inner"));
  HloComputation* middle_computation =
      module->AddEmbeddedComputation(MakeRematerializableWhileComputation(
          while_cond, /*while_body=*/inner_computation,
          /*suffix=*/".middle"));
  HloComputation* entry_computation =
      module->AddEntryComputation(MakeRematerializableWhileComputation(
          while_cond_copy, /*while_body=*/middle_computation));

  EXPECT_EQ(entry_computation->instruction_count(), 7);
  EXPECT_EQ(middle_computation->instruction_count(), 7);
  EXPECT_EQ(inner_computation->instruction_count(), 8);

  // If all computations are maximally rematerialized then peak memory usage is
  // ~12K so pick something slightly larger.
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/13 * 1024, module.get()));
  EXPECT_TRUE(changed);

  // All computations should have rematerialized instructions added.
  EXPECT_EQ(entry_computation->instruction_count(), 9);
  EXPECT_EQ(middle_computation->instruction_count(), 9);
  EXPECT_EQ(inner_computation->instruction_count(), 9);
}

TEST_F(HloRematerializationTest, RngNotRematerialized) {
  // Test that a single rng is not rematerialized:
  //
  // Entry computation:
  //   F32[] %param = {...}
  //   F32[1024] rng = rng(param)
  //   F32[1024] tanh = tanh(rng)
  //   F32[1024] exp = exp(rng)
  //   F32[1024] add_0 = add(rng, tanh)              // LIVE: add_0 + rng +
  //                                                 //       tanh + exp
  //
  //   F32[1024] add_1 = add(rng, add(exp, add_0))   // LIVE: add_1 + add_0 +
  //                                                 //       rng + tanh + exp
  //
  //   F32[1024] add_2 = add(rng, add(tanh, add_1))  // LIVE: add_2 + add_1 +
  //                                                 //       rng + tanh + exp
  auto module = CreateNewVerifiedModule();

  auto builder = HloComputation::Builder(TestName());
  auto param = builder.AddInstruction(
      HloInstruction::CreateParameter(0, scalar_shape_, "param"));
  auto rng = builder.AddInstruction(HloInstruction::CreateRng(
      vec1024_shape_, RandomDistribution::RNG_UNIFORM, {param, param}));
  auto tanh = builder.AddInstruction(
      HloInstruction::CreateUnary(vec1024_shape_, HloOpcode::kTanh, rng));
  auto exp = builder.AddInstruction(
      HloInstruction::CreateUnary(vec1024_shape_, HloOpcode::kExp, rng));
  auto add_0 = builder.AddInstruction(
      HloInstruction::CreateBinary(vec1024_shape_, HloOpcode::kAdd, rng, tanh));
  auto add_1 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, rng,
      builder.AddInstruction(HloInstruction::CreateBinary(
          vec1024_shape_, HloOpcode::kAdd, exp, add_0))));
  builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, rng,
      builder.AddInstruction(HloInstruction::CreateBinary(
          vec1024_shape_, HloOpcode::kAdd, tanh, add_1))));
  HloComputation* entry_computation =
      module->AddEntryComputation(builder.Build());

  auto count_rngs = [](const HloComputation* computation) {
    int64_t rng_count = 0;
    for (auto* instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kRng) {
        ++rng_count;
      }
    }
    return rng_count;
  };
  // Before rematerialization there should be a single broadcast rng in
  // the graph.
  ASSERT_EQ(count_rngs(entry_computation), 1);
  const int64_t original_instruction_count =
      entry_computation->instruction_count();
  // Pick a memory limit some where between 24KB (initial peak memory including
  // parameter and output) and 20KB (peak memory possible with
  // rematerialization).
  TF_ASSERT_OK_AND_ASSIGN(
      bool changed,
      RunHloRematerialization(
          /*memory_limit_bytes=*/4 * ByteSizeOf(vec1024_shape_), module.get()));
  EXPECT_TRUE(changed);
  // The rng should not have been rematerialized.
  EXPECT_EQ(count_rngs(entry_computation), 1);
  // There should have been rematerialization.
  EXPECT_GT(entry_computation->instruction_count(), original_instruction_count);
}

TEST_F(HloRematerializationTest, InstructionRematerializedMultipleTimes) {
  // Test that a single instruction is rematerialized several times. Module:
  //
  // Entry computation:
  //   F32[] %param = {...}
  //   F32[1024] %bcast = broadcast(%param)
  //   F32[1024] %add_1 = add(%bcast, bcast)
  //   F32[1024] %call_1 = call(Subcomputation, {%add_1})
  //   F32[1024] %add_2 = add(%bcast, call_1)
  //   F32[1024] %call_2 = call(SubComputation, {%add_2})
  //   F32[1024] %add_3 = add(%bcast, call_2)
  //   F32[1024] %call_3 = call(Subcomputation, {%add_3})
  //   F32[1024] %add_4 = add(%bcast, call_3)
  //
  // Subcomputation:
  //   F32[1024] %param = {...}
  //   F32[2048] %concat = concat({%param, %param})
  //   F32[1024] %slice = slice(%concat)
  //
  // The value %bcast is live across each call of Subcomputation (which requires
  // 8KB) though the value is not used in the calls. Rematerializing %bcast
  // across these calls reduces peak memory use from ~20KB down to ~16KB.
  auto module = CreateNewVerifiedModule();

  HloComputation* subcomputation = nullptr;
  {
    auto builder = HloComputation::Builder(TestName() + ".subcomputation");
    auto param = builder.AddInstruction(
        HloInstruction::CreateParameter(0, vec1024_shape_, "param"));
    auto concat = builder.AddInstruction(HloInstruction::CreateConcatenate(
        ShapeUtil::MakeShape(xla::F32, {2048}), {param, param},
        /*dimension=*/0));
    builder.AddInstruction(HloInstruction::CreateSlice(
        vec1024_shape_, concat, /*start_indices=*/{0},
        /*limit_indices=*/{1024}, /*strides=*/{1}));
    subcomputation = module->AddEmbeddedComputation(builder.Build());
  }

  auto builder = HloComputation::Builder(TestName());
  auto param = builder.AddInstruction(
      HloInstruction::CreateParameter(0, scalar_shape_, "param"));
  auto bcast = builder.AddInstruction(
      HloInstruction::CreateBroadcast(vec1024_shape_, param, {}));
  auto add_1 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, bcast, bcast));
  auto call_1 = builder.AddInstruction(
      HloInstruction::CreateCall(vec1024_shape_, {add_1}, subcomputation));
  auto add_2 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, bcast, call_1));
  auto call_2 = builder.AddInstruction(
      HloInstruction::CreateCall(vec1024_shape_, {add_2}, subcomputation));
  auto add_3 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, bcast, call_2));
  auto call_3 = builder.AddInstruction(
      HloInstruction::CreateCall(vec1024_shape_, {add_3}, subcomputation));
  auto add_4 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, bcast, call_3));
  HloComputation* entry_computation =
      module->AddEntryComputation(builder.Build());

  auto count_broadcasts = [](const HloComputation* computation) {
    int64_t bcast_count = 0;
    for (auto* instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kBroadcast) {
        bcast_count++;
      }
    }
    return bcast_count;
  };

  // Before rematerialization there should be a single broadcast instruction in
  // the graph.
  EXPECT_EQ(count_broadcasts(entry_computation), 1);
  EXPECT_EQ(entry_computation->instruction_count(), 9);

  EXPECT_EQ(add_2->operand(0), bcast);
  EXPECT_EQ(add_3->operand(0), bcast);
  EXPECT_EQ(add_4->operand(0), bcast);

  // Pick a memory limit some where between 24KB (initial peak memory including
  // parameter and output) and 20KB (peak memory possible with
  // rematerialization).
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/22 * 1024, module.get()));
  EXPECT_TRUE(changed);

  // The broadcast should have been rematerialized 3 times.
  EXPECT_EQ(count_broadcasts(entry_computation), 4);
  EXPECT_EQ(entry_computation->instruction_count(), 12);

  // The operands of add_2, add_3, and add_4 should all be rematerialized
  // broadcasts.
  EXPECT_NE(add_2->operand(0), bcast);
  EXPECT_THAT(add_2->operand(0), op::Broadcast(param));
  EXPECT_NE(add_3->operand(0), bcast);
  EXPECT_THAT(add_3->operand(0), op::Broadcast(param));
  EXPECT_NE(add_4->operand(0), bcast);
  EXPECT_THAT(add_4->operand(0), op::Broadcast(param));
}

TEST_F(HloRematerializationTest, CopyNotRematerialized) {
  // Test that copies are not rematerialized.
  auto module = CreateNewVerifiedModule();

  auto builder = HloComputation::Builder(TestName());
  auto param = builder.AddInstruction(
      HloInstruction::CreateParameter(0, vec1024_shape_, "param"));

  auto copy = builder.AddInstruction(
      HloInstruction::CreateUnary(vec1024_shape_, HloOpcode::kCopy, param));

  auto negate_a_1 = builder.AddInstruction(
      HloInstruction::CreateUnary(vec1024_shape_, HloOpcode::kNegate, copy));

  auto negate_a_2 = builder.AddInstruction(HloInstruction::CreateUnary(
      vec1024_shape_, HloOpcode::kNegate, negate_a_1));

  auto negate_b_1 = builder.AddInstruction(
      HloInstruction::CreateUnary(vec1024_shape_, HloOpcode::kNegate, copy));

  auto negate_b_2 = builder.AddInstruction(HloInstruction::CreateUnary(
      vec1024_shape_, HloOpcode::kNegate, negate_b_1));

  builder.AddInstruction(HloInstruction::CreateTuple({negate_a_2, negate_b_2}));

  HloComputation* entry_computation =
      module->AddEntryComputation(builder.Build());

  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/1 * 1024, module.get()));

  auto count_copies = [](const HloComputation* computation) {
    int64_t copy_count = 0;
    for (auto* instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kCopy) {
        copy_count++;
      }
    }
    return copy_count;
  };
  EXPECT_TRUE(changed);

  EXPECT_EQ(count_copies(entry_computation), 1);
}

class IndirectUseTest : public HloRematerializationTest,
                        public ::testing::WithParamInterface<bool> {};

TEST_P(IndirectUseTest, IndirectUseRematerialized) {
  // Test that an rematerializable instruction is rematerialized if it has
  // indirect use
  // Module:
  //
  // Entry computation:
  //   F32[] %param = {...}
  //   F32[1024] %bcast = broadcast(%param)
  //   F32[1024] %add_1 = add(%bcast, bcast)
  //   F32[1024] %call = call(Subcomputation, {%add_1})
  //   F32[1024] %add_2 = add(%bcast, call)
  //   {F32[1024], F32[1024]} %tuple = tuple(%bcast, %add_2)
  //   F32[1024] %gte = GetTupleElement(%tuple, 0)
  //   F32[1024] %negate = negate(%gte)
  //
  // Subcomputation:
  //   F32[1024] %param = {...}
  //   F32[2048] %concat = concat({%param, %param})
  //   F32[1024] %slice = slice(%concat)
  //
  // The value %bcast is live across the call and rematerialization of %bcast
  // across that point would reduce peak memory use by 4KB.
  //
  // This test is parameterized on whether the broadcast has an indirect use
  // or not. The indirect use is controlled by the index of the GetTupleElement
  // instruction. If the element is 0, then the %negate operand aliases %bcast
  // (ie %bcast is used indirectly by %negate), otherwise the %negate operand
  // aliases %add_2.
  const bool indirectly_used = GetParam();
  auto module = CreateNewVerifiedModule();

  HloComputation* subcomputation = nullptr;
  {
    auto builder = HloComputation::Builder(TestName() + ".subcomputation");
    auto param = builder.AddInstruction(
        HloInstruction::CreateParameter(0, vec1024_shape_, "param"));
    auto concat = builder.AddInstruction(HloInstruction::CreateConcatenate(
        ShapeUtil::MakeShape(xla::F32, {2048}), {param, param},
        /*dimension=*/0));
    builder.AddInstruction(HloInstruction::CreateSlice(
        vec1024_shape_, concat, /*start_indices=*/{0},
        /*limit_indices=*/{1024}, /*strides=*/{1}));
    subcomputation = module->AddEmbeddedComputation(builder.Build());
  }

  auto builder = HloComputation::Builder(TestName());
  auto param = builder.AddInstruction(
      HloInstruction::CreateParameter(0, scalar_shape_, "param"));
  auto bcast = builder.AddInstruction(
      HloInstruction::CreateBroadcast(vec1024_shape_, param, {}));
  auto add_1 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, bcast, bcast));
  auto call_1 = builder.AddInstruction(
      HloInstruction::CreateCall(vec1024_shape_, {add_1}, subcomputation));
  auto add_2 = builder.AddInstruction(HloInstruction::CreateBinary(
      vec1024_shape_, HloOpcode::kAdd, bcast, call_1));
  auto tuple =
      builder.AddInstruction(HloInstruction::CreateTuple({bcast, add_2}));
  auto gte = builder.AddInstruction(HloInstruction::CreateGetTupleElement(
      vec1024_shape_, tuple, indirectly_used ? 0 : 1));
  builder.AddInstruction(
      HloInstruction::CreateUnary(vec1024_shape_, HloOpcode::kNegate, gte));
  HloComputation* entry_computation =
      module->AddEntryComputation(builder.Build());

  EXPECT_EQ(entry_computation->instruction_count(), 8);

  // Pick a memory limit some where between 24KB (initial peak memory
  // including parameter and output) and 20KB (peak memory possible with
  // rematerialization).
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/22 * 1024, module.get()));
  // Rematerialization should only occur if the rematerializable instruction
  // has no indirect uses.
  if (indirectly_used) {
    EXPECT_TRUE(changed);
    EXPECT_EQ(entry_computation->instruction_count(), 3);
  } else {
    EXPECT_TRUE(changed);
    EXPECT_EQ(entry_computation->instruction_count(), 9);
  }
}

INSTANTIATE_TEST_SUITE_P(IndirectUseTestInstantiation, IndirectUseTest,
                         ::testing::Values(true, false));

class CompressingRematerializationTest : public RematerializationTestBase {
 protected:
  // A special shape size function, which pads the most minor dimension to 64.
  static int64_t ShapeSizePadMinorTo64(const Shape& shape) {
    if (shape.IsTuple()) {
      // Size of a tuple is 4 bytes.
      return 4;
    }
    Shape descending_shape =
        ShapeUtil::MakeShapeWithDescendingLayoutAndSamePhysicalLayout(shape);
    int64_t size =
        ShapeUtil::ByteSizeOfPrimitiveType(descending_shape.element_type());
    for (int64_t i = 0; i < descending_shape.rank(); ++i) {
      int64_t dim = descending_shape.dimensions(i);
      if (i == descending_shape.rank() - 1) {
        dim = RoundUpToNearest<int64_t>(dim, 64);
      }
      size *= dim;
    }
    return size;
  }

  // Swap the layout of the two most-minor dimensions if the second-minor
  // dimension is bigger than the most-minor dimension.
  static StatusOr<Shape> ChooseCompactLayoutForShape(const Shape& shape) {
    Shape result = shape;
    Layout layout = result.layout();
    int64_t most_minor_index = layout.minor_to_major()[0];
    int64_t second_minor_index = layout.minor_to_major()[1];
    int64_t most_minor = result.dimensions(most_minor_index);
    int64_t second_minor = result.dimensions(second_minor_index);
    if (most_minor < second_minor) {
      Layout new_layout = layout;
      new_layout.set_minor_to_major(0, second_minor_index);
      new_layout.set_minor_to_major(1, most_minor_index);
      *result.mutable_layout() = new_layout;
    }
    return result;
  }

  StatusOr<bool> RunHloRematerialization(int64_t memory_limit_bytes,
                                         HloModule* module,
                                         int64_t min_remat_size = 0) {
    TF_EXPECT_OK(verifier().Run(module).status());
    HloRematerialization remat(
        ShapeSizePadMinorTo64, memory_limit_bytes,
        /*sizes=*/nullptr,
        HloRematerialization::RematerializationPass::kPreFusion,
        /*block_size_limit=*/1, /*block_rematerialization_factor=*/1,
        ChooseCompactLayoutForShape,
        HloRematerialization::RematerializationMode::kCompressOnly,
        min_remat_size);
    return remat.Run(module);
  }
};

// Test rematerialization only remats big buffer that pass certain limits.
TEST_F(CompressingRematerializationTest, OnlyRematBigBuffer) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_float {
  %x = f32[] parameter(0)
  %y = f32[] parameter(1)
  ROOT %add = f32[] add(f32[] %x, f32[] %y)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %constant = f32[] constant(0)
  %broadcast.0 = f32[64,2]{1,0} broadcast(f32[] %param.0), dimensions={}
  %broadcast.1 = f32[10,2]{1,0} broadcast(f32[] %param.0), dimensions={}
  %negate = f32[64,2]{1,0} negate(f32[64,2]{1,0} broadcast.0)
  %reduce.0 = f32[] reduce(f32[64,2]{1,0} %negate, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %reduce.1 = f32[] reduce(f32[64,2]{1,0} %broadcast.0, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %reduce.2 = f32[] reduce(f32[10,2]{1,0} %broadcast.1, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %add = f32[] add(f32[] %reduce.0, f32[] %reduce.1)
  ROOT %add.2 = f32[] add(f32[] %add, f32[] %reduce.2)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));

  // Only rematerialize buffers which have shaep f32[64, 2]. Buffers with shape
  // f32[10, 2] are ignored.
  TF_ASSERT_OK_AND_ASSIGN(bool changed, RunHloRematerialization(
                                            /*memory_limit_bytes=*/30 * 1024,
                                            module.get(), 10 * 1024));
  EXPECT_TRUE(changed);
  HloInstruction* broadcast =
      module->entry_computation()->GetInstructionWithName("broadcast.0");
  HloInstruction* broadcast_2 =
      module->entry_computation()->GetInstructionWithName("broadcast.1");
  HloInstruction* reduce =
      module->entry_computation()->GetInstructionWithName("reduce.1");
  HloInstruction* reduce_2 =
      module->entry_computation()->GetInstructionWithName("reduce.2");
  EXPECT_THAT(reduce,
              op::Reduce(op::Copy(op::Copy(broadcast)), op::Constant()));
  EXPECT_THAT(reduce_2, op::Reduce(broadcast_2, op::Constant()));
}

// Test rematerialization of a single instruction.
TEST_F(CompressingRematerializationTest, SingleRemat) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_float {
  %x = f32[] parameter(0)
  %y = f32[] parameter(1)
  ROOT %add = f32[] add(f32[] %x, f32[] %y)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %constant = f32[] constant(0)
  %broadcast.0 = f32[64,2]{1,0} broadcast(f32[] %param.0), dimensions={}
  %negate = f32[64,2]{1,0} negate(f32[64,2]{1,0} broadcast.0)
  %reduce.0 = f32[] reduce(f32[64,2]{1,0} %negate, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %reduce.1 = f32[] reduce(f32[64,2]{1,0} %broadcast.0, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %add = f32[] add(f32[] %reduce.0, f32[] %reduce.1)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));

  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/30 * 1024, module.get()));
  EXPECT_TRUE(changed);
  HloInstruction* broadcast =
      module->entry_computation()->GetInstructionWithName("broadcast.0");
  HloInstruction* reduce =
      module->entry_computation()->GetInstructionWithName("reduce.1");
  EXPECT_THAT(reduce,
              op::Reduce(op::Copy(op::Copy(broadcast)), op::Constant()));
}

TEST_F(CompressingRematerializationTest, AllUsersUseSameCopy) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_float {
  %x = f32[] parameter(0)
  %y = f32[] parameter(1)
  ROOT %add = f32[] add(f32[] %x, f32[] %y)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %constant = f32[] constant(0)
  %broadcast.0 = f32[64,2]{1,0} broadcast(f32[] %param.0), dimensions={}
  %negate = f32[64,2]{1,0} negate(f32[64,2]{1,0} broadcast.0)
  %reduce.0 = f32[] reduce(f32[64,2]{1,0} %negate, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %reduce.1 = f32[] reduce(f32[64,2]{1,0} %negate, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %reduce.2 = f32[] reduce(f32[64,2]{1,0} %broadcast.0, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %add = f32[] add(f32[] %reduce.0, f32[] %reduce.1)
  %reduce.3 = f32[] reduce(f32[64,2]{1,0} %broadcast.0, f32[] %constant), dimensions={1, 0}, to_apply=%add_float
  %add.2 = f32[] add(f32[] %reduce.2, f32[] %reduce.3)
  ROOT %tuple = (f32[], f32[]) tuple (f32[] add, f32[] add.2)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));

  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/30 * 1024, module.get()));
  EXPECT_TRUE(changed);

  HloInstruction* broadcast =
      module->entry_computation()->GetInstructionWithName("broadcast.0");

  // Both reduces reuse the same copy instruction.
  HloInstruction* reduce_2 =
      module->entry_computation()->GetInstructionWithName("reduce.2");

  HloInstruction* reduce_3 =
      module->entry_computation()->GetInstructionWithName("reduce.3");

  EXPECT_THAT(reduce_2,
              op::Reduce(op::Copy(op::Copy(broadcast)), op::Constant()));

  EXPECT_THAT(reduce_3,
              op::Reduce(op::Copy(op::Copy(broadcast)), op::Constant()));
}

// Test rematerialization of values through bitcasts
// Its expected that the broadcast gets rematerialized
TEST_F(HloRematerializationTest, ThroughBitcastRemat) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

ENTRY %mycomp (param: f32[1]) -> f32[1] {
  %param = f32[1]{0} parameter(0)
  %reshape = f32[] reshape(f32[1]{0} %param)
  %broadcast = f32[1024,1]{1,0} broadcast(f32[] %reshape), dimensions={}
  %bitcast = f32[1024]{0} bitcast(f32[1024,1]{1,0} %broadcast)
  %negate = f32[1024,1]{1,0} negate(f32[1024,1]{1,0} %broadcast)
  %concatenate = f32[2048,1]{1,0} concatenate(f32[1024,1]{1,0} %negate, f32[1024,1]{1,0} %negate), dimensions={0}
  %slice = f32[1,1]{1,0} slice(f32[2048,1]{1,0} %concatenate), slice={[0:1], [0:1]}
  %bitcast.1 = f32[1]{0} bitcast(f32[1,1]{1,0} %slice)
  %concatenate.1 = f32[1025]{0} concatenate(f32[1024]{0} %bitcast, f32[1]{0} %bitcast.1), dimensions={0}
  ROOT %slice.1 = f32[1]{0} slice(f32[1025]{0} %concatenate.1), slice={[0:1]}
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));

  auto* computation = module->entry_computation();
  // Find and save the original broadcast instruction which should be
  // rematerialized.
  const HloInstruction* slice = computation->root_instruction();
  ASSERT_THAT(slice,
              op::Slice(op::Concatenate(op::Bitcast(op::Broadcast(_)), _)));
  const HloInstruction* concat = slice->operand(0);
  const HloInstruction* bcast = concat->operand(0)->operand(0);

  // Computation requires 16KB without rematerialization, but uses only 12KB
  // with rematerialization so pick a memory limit between these values (14KB).
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/14 * 1024, module.get()));
  EXPECT_TRUE(changed);

  // Root should not have changed.
  EXPECT_EQ(computation->root_instruction(), slice);

  // The bitcast for the rematerialized broadcast
  const HloInstruction* remat_bitcast = concat->operand(0);
  // The broadcast should have been rematerialized.
  const HloInstruction* remat_broadcast = remat_bitcast->operand(0);

  EXPECT_THAT(remat_broadcast, op::Broadcast(::testing::Ne(bcast)));

  // The rematerialized broadcast should be immediately before its bitcast
  // and the bitcast before the concatenate in the sequence.
  EXPECT_EQ(module->schedule()
                .sequence(computation)
                .instructions()[computation->instruction_count() - 2],
            concat);
  EXPECT_EQ(module->schedule()
                .sequence(computation)
                .instructions()[computation->instruction_count() - 3],
            remat_bitcast);
  EXPECT_EQ(module->schedule()
                .sequence(computation)
                .instructions()[computation->instruction_count() - 4],
            remat_broadcast);
}

// Test that the "deny list for move remats" engages when we rematerialize
// through bitcasts.
TEST_F(HloRematerializationTest, ThroughBitcastRematInfiniteLoop) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

ENTRY %mycomp (param: f32[1]) -> f32[1024] {
  %param = f32[1]{0} parameter(0)
  %reshape = f32[] reshape(f32[1]{0} %param)
  %broadcast = f32[1024,1]{1,0} broadcast(f32[] %reshape), dimensions={}
  %bitcast = f32[1024]{0} bitcast(f32[1024,1]{1,0} %broadcast)
  %broadcast2 = f32[1024,1]{1,0} broadcast(f32[] %reshape), dimensions={}
  %bitcast2 = f32[1024]{0} bitcast(f32[1024,1]{1,0} %broadcast2)
  ROOT %add = f32[1024]{0} add(f32[1024]{0} %bitcast, f32[1024]{0} %bitcast2)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));

  auto* computation = module->entry_computation();
  // Find and save the original broadcasts instruction which should be
  // rematerialized.
  const HloInstruction* add = computation->root_instruction();
  // Run with a low rematerialization limit that cannot be satisfied to make
  // sure that we don't get stuck in a loop trying to lower it.
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/1024, module.get()));
  ASSERT_THAT(add, op::Add(op::Bitcast(op::Broadcast(_)),
                           op::Bitcast(op::Broadcast(_))));
  EXPECT_TRUE(changed);
}

TEST_F(HloRematerializationTest, RematTupleShape) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_mul_comp {
  %p0 = f32[] parameter(0)
  %p1 = f32[] parameter(1)
  %x = f32[1024]{0} broadcast(f32[] %p0), dimensions={}
  %y = f32[1024]{0} broadcast(f32[] %p1), dimensions={}
  %add = f32[1024] add(%x, %y)
  %mul = f32[1024] multiply(%x, %y)
  ROOT %out = (f32[1024], f32[1024]) tuple(%add, %mul)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %param.1 = f32[] parameter(1)
  %fus = (f32[1024]{0}, f32[1024]{0}) fusion(%param.0, %param.1), kind=kLoop,
    calls=%add_mul_comp
  %gte.1 = f32[1024]{0} get-tuple-element(%fus), index=0
  %add = f32[1024]{0} add(f32[1024]{0} %gte.1, f32[1024]{0} %gte.1)
  %broadcast.1 = f32[1024]{0} broadcast(f32[] %param.0), dimensions={}
  %mul = f32[1024]{0} multiply(f32[1024]{0} %add, f32[1024]{0} %broadcast.1)
  %gte.2 = f32[1024]{0} get-tuple-element(%fus), index=1
  ROOT %add.2 = f32[1024]{0} add(f32[1024]{0} %mul, f32[1024]{0} %gte.2)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));
  const HloComputation* computation = module->entry_computation();
  const HloInstruction* add = computation->root_instruction();
  ASSERT_THAT(add, op::Add(op::Multiply(), op::GetTupleElement(op::Fusion())));
  const HloInstruction* fusion = add->operand(0)->operand(0);
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/11 * 1024, module.get()));
  EXPECT_TRUE(changed);
  ASSERT_THAT(
      add, op::Add(op::Multiply(), op::GetTupleElement(AllOf(
                                       op::Fusion(), ::testing::Ne(fusion)))));
}

TEST_F(HloRematerializationTest, RematTupleShapeDoubleUse) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_mul_comp {
  %p0 = f32[] parameter(0)
  %p1 = f32[] parameter(1)
  %x = f32[1024]{0} broadcast(f32[] %p0), dimensions={}
  %y = f32[1024]{0} broadcast(f32[] %p1), dimensions={}
  %add = f32[1024] add(%x, %y)
  %mul = f32[1024] multiply(%x, %y)
  ROOT %out = (f32[1024], f32[1024]) tuple(%add, %mul)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %param.1 = f32[] parameter(1)
  %fus = (f32[1024]{0}, f32[1024]{0}) fusion(%param.0, %param.1), kind=kLoop,
    calls=%add_mul_comp
  %gte.1 = f32[1024]{0} get-tuple-element(%fus), index=0
  %add = f32[1024]{0} add(f32[1024]{0} %gte.1, f32[1024]{0} %gte.1)
  %broadcast.1 = f32[1024]{0} broadcast(f32[] %param.0), dimensions={}
  %mul = f32[1024]{0} multiply(f32[1024]{0} %add, f32[1024]{0} %broadcast.1)
  %gte.2 = f32[1024]{0} get-tuple-element(%fus), index=1
  %gte.3 = f32[1024]{0} get-tuple-element(%fus), index=0
  %add.2 = f32[1024]{0} add(f32[1024]{0} %mul, f32[1024]{0} %gte.2)
  ROOT %mul.2 = f32[1024]{0} multiply(f32[1024]{0} %add.2, f32[1024]{0} %gte.3)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));
  const HloComputation* computation = module->entry_computation();
  const HloInstruction* add = computation->root_instruction();
  ASSERT_THAT(add, op::Multiply(op::Add(op::Multiply(),
                                        op::GetTupleElement(op::Fusion())),
                                op::GetTupleElement(op::Fusion())));
  const HloInstruction* fusion = add->operand(0)->operand(0);
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/11 * 1024, module.get()));
  EXPECT_TRUE(changed);
  ASSERT_THAT(
      add,
      op::Multiply(
          op::Add(op::Multiply(), op::GetTupleElement(AllOf(
                                      op::Fusion(), ::testing::Ne(fusion)))),
          op::GetTupleElement(AllOf(op::Fusion(), ::testing::Ne(fusion)))));
  // Check that the rematerialized fusion is the same for both ops.
  EXPECT_EQ(add->operand(0)->operand(1)->operand(0),
            add->operand(1)->operand(0));
}

TEST_F(HloRematerializationTest, RematTupleShapeThroughBitcasts) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_mul_comp {
  %p0 = f32[] parameter(0)
  %p1 = f32[] parameter(1)
  %x = f32[1024]{0} broadcast(f32[] %p0), dimensions={}
  %y = f32[1024]{0} broadcast(f32[] %p1), dimensions={}
  %add = f32[1024] add(%x, %y)
  %mul = f32[1024] multiply(%x, %y)
  ROOT %out = (f32[1024], f32[1024]) tuple(%add, %mul)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %param.1 = f32[] parameter(1)
  %fus = (f32[1024]{0}, f32[1024]{0}) fusion(%param.0, %param.1), kind=kLoop,
    calls=%add_mul_comp
  %gte.1 = f32[1024]{0} get-tuple-element(%fus), index=0
  %add = f32[1024]{0} add(f32[1024]{0} %gte.1, f32[1024]{0} %gte.1)
  %broadcast.1 = f32[1024]{0} broadcast(f32[] %param.0), dimensions={}
  %mul = f32[1024]{0} multiply(f32[1024]{0} %add, f32[1024]{0} %broadcast.1)
  %gte.2 = f32[1024]{0} get-tuple-element(%fus), index=1
  %bc.1 = f32[1024,1]{0,1} bitcast(%mul)
  %bc.2 = f32[1024,1]{0,1} bitcast(%gte.2)
  ROOT %add.2 = f32[1024,1]{0,1} add(f32[1024,1]{0,1} %bc.1,
    f32[1024,1]{0,1} %bc.2)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));
  const HloComputation* computation = module->entry_computation();
  const HloInstruction* add = computation->root_instruction();
  ASSERT_THAT(add, op::Add(op::Bitcast(op::Multiply()),
                           op::Bitcast(op::GetTupleElement(op::Fusion()))));
  const HloInstruction* fusion = add->operand(0)->operand(0)->operand(0);
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/11 * 1024, module.get()));
  EXPECT_TRUE(changed);
  ASSERT_THAT(add, op::Add(op::Bitcast(op::Multiply()),
                           op::Bitcast(op::GetTupleElement(
                               AllOf(op::Fusion(), ::testing::Ne(fusion))))));
}

TEST_F(HloRematerializationTest, RematThroughTuple) {
  const string& hlo_string = R"(
HloModule fusion, is_scheduled=true

%add_mul_comp {
  %p0 = f32[] parameter(0)
  %p1 = f32[] parameter(1)
  %x = f32[1024]{0} broadcast(f32[] %p0), dimensions={}
  %y = f32[1024]{0} broadcast(f32[] %p1), dimensions={}
  %add = f32[1024] add(%x, %y)
  %mul = f32[1024] multiply(%x, %y)
  ROOT %out = (f32[1024], f32[1024]) tuple(%add, %mul)
}

ENTRY %entry {
  %param.0 = f32[] parameter(0)
  %param.1 = f32[] parameter(1)
  %fus = (f32[1024]{0}, f32[1024]{0}) fusion(%param.0, %param.1), kind=kLoop,
    calls=%add_mul_comp
  %gte.1 = f32[1024]{0} get-tuple-element(%fus), index=0
  %gte.3 = f32[1024]{0} get-tuple-element(%fus), index=1
  %add = f32[1024]{0} add(f32[1024]{0} %gte.1, f32[1024]{0} %gte.3)
  %broadcast.1 = f32[1024]{0} broadcast(f32[] %param.0), dimensions={}
  %mul = f32[1024]{0} multiply(f32[1024]{0} %add, f32[1024]{0} %broadcast.1)
  %tpl = (f32[1024]{0}, f32[1024]{0}) tuple(%gte.1, %add)
  %bc.1 = f32[1024,1]{0,1} bitcast(%mul)
  %gte.2 = f32[1024]{0} get-tuple-element(%tpl), index=0
  ROOT %add.2 = f32[1024]{0} add(f32[1024]{0} %gte.2, f32[1024]{0} %add)
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));

  const HloComputation* computation = module->entry_computation();
  const HloInstruction* add = computation->root_instruction();
  ASSERT_THAT(add, op::Add(op::GetTupleElement(
                               op::Tuple(op::GetTupleElement(op::Fusion()), _)),
                           op::Add()));
  const HloInstruction* tuple = add->operand(0)->operand(0);
  const HloInstruction* fusion = tuple->operand(0)->operand(0);
  TF_ASSERT_OK_AND_ASSIGN(bool changed,
                          RunHloRematerialization(
                              /*memory_limit_bytes=*/11 * 1024, module.get()));
  EXPECT_TRUE(changed);
  ASSERT_THAT(
      add, op::Add(op::GetTupleElement(AllOf(op::Fusion(), ::testing::Ne(tuple),
                                             ::testing::Ne(fusion))),
                   op::Add()));
}
}  // namespace

}  // namespace xla
