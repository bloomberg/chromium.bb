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

#include <utility>

#include "tensorflow/compiler/xla/service/gpu/gpu_executable.h"
#include "tensorflow/compiler/xla/service/gpu/tests/gpu_codegen_test.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_module_config.h"
#include "tensorflow/compiler/xla/service/hlo_parser.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/tests/filecheck.h"
#include "tensorflow/compiler/xla/tests/hlo_test_base.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/stream_executor/lib/statusor.h"

namespace xla {
namespace gpu {

namespace {

class ReductionDegenerateDimRemoverTest : public GpuCodegenTest {
  DebugOptions GetDebugOptionsForTest() override {
    DebugOptions debug_options = GpuCodegenTest::GetDebugOptionsForTest();
    debug_options.add_xla_disable_hlo_passes("reduction-layout-normalizer");
    debug_options.add_xla_disable_hlo_passes("reduction-dimension-grouper");
    debug_options.add_xla_disable_hlo_passes("reduction-splitter");
    debug_options.add_xla_disable_hlo_passes("gpu-tree-reduction-rewriter");
    return debug_options;
  }
};

TEST_F(ReductionDegenerateDimRemoverTest, ReductionWithDegenerateDimensions) {
  const char* hlo_text = R"(
HloModule ReduceWithDegenerateDimensions

add {
  accum = f32[] parameter(0)
  op = f32[] parameter(1)
  ROOT out = f32[] add(accum, op)
}

ENTRY main {
  input = f32[1,3,1,4,1,5,1] parameter(0)
  zero = f32[] constant(0)

  ROOT out = f32[1,1,1,1] reduce(input, zero), dimensions={1,3,5}, to_apply=add
}

)";

  EXPECT_TRUE(RunAndCompare(hlo_text, ErrorSpec{1e-5, 1e-5}));
  MatchOptimizedHloWithShapes(hlo_text,
                              R"(
// CHECK: f32[] reduce(f32[3,4,5]{2,1,0} {{.+}}, f32[] {{.+}}), dimensions={0,1,2}, to_apply=%add
      )");
}

TEST_F(ReductionDegenerateDimRemoverTest,
       ReductionWithDegenerateDimensionsVariadic) {
  const char* hlo_text = R"(
HloModule ReduceWithDegenerateDimensions

argmax {
  running_max = f32[] parameter(0)
  running_max_idx = u32[] parameter(1)
  current_value = f32[] parameter(2)
  current_value_idx = u32[] parameter(3)

  current = (f32[], u32[]) tuple(running_max, running_max_idx)
  potential = (f32[], u32[]) tuple(current_value, current_value_idx)

  cmp_code = pred[] compare(current_value, running_max), direction=GT

  new_max = f32[] select(cmp_code, current_value, running_max)
  new_idx = u32[] select(cmp_code, current_value_idx, running_max_idx)

  ROOT out = (f32[], u32[]) tuple(new_max, new_idx)
}

ENTRY main {
  input = f32[1,3,1,4,1,5,1] parameter(0)
  idxs = u32[1,3,1,4,1,5,1] parameter(1)
  zero = f32[] constant(0)
  zero_idx = u32[] constant(0)

  ROOT out = (f32[1,1,1,1], u32[1,1,1,1]) reduce(input, idxs, zero, zero_idx), dimensions={1,3,5}, to_apply=argmax
}

)";

  EXPECT_TRUE(RunAndCompare(hlo_text, ErrorSpec{1e-5, 1e-5}));
  MatchOptimizedHloWithShapes(hlo_text,
                              R"(
// CHECK: (f32[], u32[]) reduce(f32[3,4,5]{2,1,0} %bitcast, u32[3,4,5]{2,1,0} %bitcast.1, f32[] %zero, u32[] %zero_idx), dimensions={0,1,2}
)");
}

TEST_F(ReductionDegenerateDimRemoverTest, DegenerateWithEmptyDimension) {
  const char* hlo_text = R"(
HloModule ReduceWithDegenerateDimensions

add {
  accum = f32[] parameter(0)
  op = f32[] parameter(1)
  ROOT out = f32[] add(accum, op)
}

ENTRY main {
  input = f32[1,3,1,4,1,5,1] parameter(0)
  zero = f32[] constant(0)

  ROOT out = f32[3,4,5,1] reduce(input, zero), dimensions={0,2,4}, to_apply=add
}

)";

  EXPECT_TRUE(RunAndCompare(hlo_text, ErrorSpec{1e-5, 1e-5}));
  // Copy instruction is added after bitcast because of copy-insertion pass,
  // so we check the entire hlo module to verify there is no reduce instruction
  // in this case.
  MatchOptimizedHloWithShapes(hlo_text,
                              R"(
// CHECK: ENTRY %main (input: f32[1,3,1,4,1,5,1]) -> f32[3,4,5,1] {
// CHECK:   %input = f32[1,3,1,4,1,5,1]{6,5,4,3,2,1,0} parameter(0)
// CHECK:   %bitcast{{.+}} = f32[3,4,5,1]{3,2,1,0} bitcast(f32[1,3,1,4,1,5,1]{6,5,4,3,2,1,0} %input)
// CHECK:   ROOT %copy{{.+}} = f32[3,4,5,1]{3,2,1,0} copy(f32[3,4,5,1]{3,2,1,0} %bitcast{{.+}})
      )");
}

}  // namespace
}  // namespace gpu
}  // namespace xla
