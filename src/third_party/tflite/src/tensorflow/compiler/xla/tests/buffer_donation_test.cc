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

#include <vector>

#include "absl/memory/memory.h"
#include "tensorflow/compiler/xla/client/client_library.h"
#include "tensorflow/compiler/xla/client/local_client.h"
#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/service/backend.h"
#include "tensorflow/compiler/xla/service/executable.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/tests/hlo_test_base.h"
#include "tensorflow/compiler/xla/tests/literal_test_util.h"
#include "tensorflow/core/lib/core/status_test_util.h"

namespace xla {
namespace {

// This test runs a computation and reuses different subsets of
// input buffers as output buffers. The aliasing patterns executed
// are as follows:
// 1. output[0] == input[0], output[1] == input[1], output[2] == input[2]
// 2. output[0] == input[1], output[1] == input[2].
// 3. output[0] == input[2]
class BufferDonationTest : public HloTestBase {
 public:
  BufferDonationTest() {
    client_ = ClientLibrary::LocalClientOrDie();
    backend_ = &client_->backend();
    platform_ = backend_->platform();
    executor_ = backend_->default_stream_executor();
    TF_CHECK_OK(executor_->Init());
  }

 protected:
  LocalClient* client_;
  se::Platform* platform_;
  const Backend* backend_;
  se::StreamExecutor* executor_;

  void RunAndCheck(std::unique_ptr<HloModule> hlo_module,
                   const Literal& argument_literal, Literal* expected) {
    // Create a copy of the output shape because the HLO module is std::moved
    // into the compiler and may be deallocated.
    const Shape output_shape = hlo_module->result_shape();

    TF_ASSERT_OK_AND_ASSIGN(hlo_module, backend_->compiler()->RunHloPasses(
                                            std::move(hlo_module), executor_,
                                            /*device_allocator=*/nullptr));
    TF_ASSERT_OK_AND_ASSIGN(
        std::unique_ptr<Executable> executable,
        backend_->compiler()->RunBackend(std::move(hlo_module), executor_,
                                         /*device_allocator=*/nullptr));

    se::Stream stream(executor_);
    ASSERT_TRUE(stream.Init().ok());

    auto memory_allocator =
        absl::make_unique<se::StreamExecutorMemoryAllocator>(
            platform_, backend_->stream_executors());
    ExecutableRunOptions run_options;
    run_options.set_stream(&stream);
    run_options.set_allocator(memory_allocator.get());
    ServiceExecutableRunOptions service_run_options(run_options);

    // Allocate input buffers that will be reused as outputs.
    TF_ASSERT_OK_AND_ASSIGN(
        auto scoped_shaped_buffer,
        backend_->transfer_manager()->AllocateScopedShapedBuffer(
            argument_literal.shape(), memory_allocator.get(),
            executor_->device_ordinal()));
    auto shaped_buffer = scoped_shaped_buffer.release();
    TF_CHECK_OK(backend_->transfer_manager()->TransferLiteralToDevice(
        &stream, argument_literal, shaped_buffer));
    auto input_buffers = shaped_buffer.buffers();
    ShapeTree<MaybeOwningDeviceMemory> owned_buffers(argument_literal.shape());
    owned_buffers.ForEachMutableElement(
        [&](const ShapeIndex& index, MaybeOwningDeviceMemory* device_memory) {
          *device_memory = se::OwningDeviceMemory(input_buffers.element(index),
                                                  executor_->device_ordinal(),
                                                  memory_allocator.get());
        });

    std::vector<ExecutionInput> args;
    args.emplace_back(ExecutionInput(std::move(owned_buffers)));

    TF_ASSERT_OK_AND_ASSIGN(
        ExecutionOutput output,
        executable->ExecuteAsyncOnStream(&service_run_options, std::move(args),
                                         /*hlo_execution_profile=*/nullptr));

    se::DeviceMemoryBase result_root_buffer = output.Result().root_buffer();
    LOG(INFO) << "result allocation = " << result_root_buffer.opaque()
              << "             size = " << result_root_buffer.size();

    // Check for expected aliasing between input and output buffers.
#ifndef XLA_TEST_BACKEND_INTERPRETER
    for (int i = 0; i < ShapeUtil::TupleElementCount(argument_literal.shape());
         ++i) {
      const ShapeIndex index({i});
      if (input_buffers.element(index).size() ==
          output.Result().buffer(index).size()) {
        ASSERT_EQ(input_buffers.element(index).opaque(),
                  output.Result().buffer(index).opaque());
      } else {
        ASSERT_NE(input_buffers.element(index).opaque(),
                  output.Result().buffer(index).opaque());
      }
    }
#endif

    TF_ASSERT_OK(run_options.stream()->BlockHostUntilDone());
    TF_ASSERT_OK_AND_ASSIGN(
        Literal result_literal,
        backend_->transfer_manager()->TransferLiteralFromDevice(
            &stream, output.Result()));
    EXPECT_TRUE(LiteralTestUtil::Equal(*expected, result_literal));

    // Memories are automatically deallocated.
  }

  // Builds a simple compare-to-limit (x < 4) computation for a While.
  //
  // condition:
  //   const4[s32] -----------------------------------\
  //                                                   \
  //   param[(s32,f32[4])] --- get-tuple-element[0] --- less-than
  //
  std::unique_ptr<HloComputation> BuildWhileConditionComputation(
      const string& name) {
    auto builder = HloComputation::Builder(name);
    auto const4 = builder.AddInstruction(
        HloInstruction::CreateConstant(LiteralUtil::CreateR0<int>(4)));
    auto param = builder.AddInstruction(
        HloInstruction::CreateParameter(0, t_s32_f32v1_, "x"));
    auto index = builder.AddInstruction(
        HloInstruction::CreateGetTupleElement(const4->shape(), param, 0));
    builder.AddInstruction(
        HloInstruction::CreateCompare(ShapeUtil::MakeShape(PRED, {}), index,
                                      const4, ComparisonDirection::kLt));
    return builder.Build();
  }

  // Builds a simple body computation for a While.
  //
  // body:
  //   constv[f32[1]] --------------------------------------\
  //                                                         \
  //                           /--- get-tuple-elementv[1] --- addv ---\
  //   param[(s32,f32[1])] ---|                                    tuple
  //                           \--- get-tuple-elementc[0] --- addc ---/
  //                                                         /
  //   const1[s32] -----------------------------------------/
  //
  std::unique_ptr<HloComputation> BuildWhileBodyComputation(
      const string& name) {
    auto builder = HloComputation::Builder(name);
    auto const1 = builder.AddInstruction(
        HloInstruction::CreateConstant(LiteralUtil::CreateR0<int>(1)));
    auto constv = builder.AddInstruction(
        HloInstruction::CreateConstant(LiteralUtil::CreateR1<float>({1.1f})));
    auto param = builder.AddInstruction(
        HloInstruction::CreateParameter(0, t_s32_f32v1_, "x"));
    auto indexc = builder.AddInstruction(
        HloInstruction::CreateGetTupleElement(const1->shape(), param, 0));
    auto addc = builder.AddInstruction(HloInstruction::CreateBinary(
        indexc->shape(), HloOpcode::kAdd, indexc, const1));
    auto indexv = builder.AddInstruction(
        HloInstruction::CreateGetTupleElement(constv->shape(), param, 1));
    auto addv = builder.AddInstruction(HloInstruction::CreateBinary(
        constv->shape(), HloOpcode::kAdd, indexv, constv));
    builder.AddInstruction(HloInstruction::CreateTuple({addc, addv}));
    return builder.Build();
  }

  Shape s32_ = ShapeUtil::MakeShape(xla::S32, {});
  Shape r0f32_ = ShapeUtil::MakeShape(xla::F32, {});
  Shape f32v1_ = ShapeUtil::MakeShape(F32, {1});
  Shape t_s32_f32v1_ = ShapeUtil::MakeTupleShape({s32_, f32v1_});
};

// This tests a simple while loop where the parameters are aliased with the
// output buffers.
TEST_F(BufferDonationTest, SimpleWhileTupleTest) {
  auto module = CreateNewVerifiedModule("SimpleWhile");
  auto condition =
      module->AddEmbeddedComputation(BuildWhileConditionComputation("if<4"));
  auto body =
      module->AddEmbeddedComputation(BuildWhileBodyComputation("add-update"));

  auto builder = HloComputation::Builder("SimpleWhile");
  auto param = builder.AddInstruction(
      HloInstruction::CreateParameter(0, t_s32_f32v1_, "param"));
  auto while0 = builder.AddInstruction(
      HloInstruction::CreateWhile(t_s32_f32v1_, condition, body, param));
  auto gte0 = builder.AddInstruction(
      HloInstruction::CreateGetTupleElement(s32_, while0, 0));
  auto gte1 = builder.AddInstruction(
      HloInstruction::CreateGetTupleElement(f32v1_, while0, 1));
  builder.AddInstruction(HloInstruction::CreateTuple({gte0, gte1}));
  module->AddEntryComputation(builder.Build());
  TF_ASSERT_OK(module->input_output_alias_config().SetUpAlias({0}, 0, {0}));
  TF_ASSERT_OK(module->input_output_alias_config().SetUpAlias({1}, 0, {1}));

  auto arg = LiteralUtil::MakeTupleFromSlices(
      {LiteralUtil::CreateR0<int>(0), LiteralUtil::CreateR1<float>({1.1f})});
  auto expected = LiteralUtil::MakeTupleFromSlices(
      {LiteralUtil::CreateR0<int>(4), LiteralUtil::CreateR1<float>({5.5f})});
  RunAndCheck(std::move(module), arg, &expected);
}

}  // namespace
}  // namespace xla
