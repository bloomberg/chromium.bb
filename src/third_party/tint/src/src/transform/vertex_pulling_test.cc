// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/transform/vertex_pulling.h"

#include <utility>

#include "src/transform/test_helper.h"

namespace tint {
namespace transform {
namespace {

using VertexPullingTest = TransformTest;

TEST_F(VertexPullingTest, Error_NoEntryPoint) {
  auto* src = "";

  auto* expect = "error: Vertex stage entry point not found";

  VertexPulling::Config cfg;

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, Error_InvalidEntryPoint) {
  auto* src = R"(
[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = "error: Vertex stage entry point not found";

  VertexPulling::Config cfg;
  cfg.entry_point_name = "_";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, Error_EntryPointWrongStage) {
  auto* src = R"(
[[stage(fragment)]]
fn main() -> void {}
)";

  auto* expect = "error: Vertex stage entry point not found";

  VertexPulling::Config cfg;
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, BasicModule) {
  auto* src = R"(
[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
  }
}
)";

  VertexPulling::Config cfg;
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, OneAttribute) {
  auto* src = R"(
[[location(0)]] var<in> var_a : f32;

[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[builtin(vertex_index)]] var<in> _tint_pulling_vertex_index : u32;

[[binding(0), group(4)]] var<storage> _tint_pulling_vertex_buffer_0 : TintVertexData;

[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

var<private> var_a : f32;

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 4u) + 0u);
    var_a = bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[(_tint_pulling_pos / 4u)]);
  }
}
)";

  VertexPulling::Config cfg;
  cfg.vertex_state = {
      {{4, InputStepMode::kVertex, {{VertexFormat::kF32, 0, 0}}}}};
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, OneInstancedAttribute) {
  auto* src = R"(
[[location(0)]] var<in> var_a : f32;

[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[builtin(instance_index)]] var<in> _tint_pulling_instance_index : u32;

[[binding(0), group(4)]] var<storage> _tint_pulling_vertex_buffer_0 : TintVertexData;

[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

var<private> var_a : f32;

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
    _tint_pulling_pos = ((_tint_pulling_instance_index * 4u) + 0u);
    var_a = bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[(_tint_pulling_pos / 4u)]);
  }
}
)";

  VertexPulling::Config cfg;
  cfg.vertex_state = {
      {{4, InputStepMode::kInstance, {{VertexFormat::kF32, 0, 0}}}}};
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, OneAttributeDifferentOutputSet) {
  auto* src = R"(
[[location(0)]] var<in> var_a : f32;

[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[builtin(vertex_index)]] var<in> _tint_pulling_vertex_index : u32;

[[binding(0), group(5)]] var<storage> _tint_pulling_vertex_buffer_0 : TintVertexData;

[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

var<private> var_a : f32;

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 4u) + 0u);
    var_a = bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[(_tint_pulling_pos / 4u)]);
  }
}
)";

  VertexPulling::Config cfg;
  cfg.vertex_state = {
      {{4, InputStepMode::kVertex, {{VertexFormat::kF32, 0, 0}}}}};
  cfg.pulling_group = 5;
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

// We expect the transform to use an existing builtin variables if it finds them
TEST_F(VertexPullingTest, ExistingVertexIndexAndInstanceIndex) {
  auto* src = R"(
[[location(0)]] var<in> var_a : f32;
[[location(1)]] var<in> var_b : f32;
[[builtin(vertex_index)]] var<in> custom_vertex_index : u32;
[[builtin(instance_index)]] var<in> custom_instance_index : u32;

[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[binding(0), group(4)]] var<storage> _tint_pulling_vertex_buffer_0 : TintVertexData;

[[binding(1), group(4)]] var<storage> _tint_pulling_vertex_buffer_1 : TintVertexData;

[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

var<private> var_a : f32;

var<private> var_b : f32;

[[builtin(vertex_index)]] var<in> custom_vertex_index : u32;

[[builtin(instance_index)]] var<in> custom_instance_index : u32;

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
    _tint_pulling_pos = ((custom_vertex_index * 4u) + 0u);
    var_a = bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[(_tint_pulling_pos / 4u)]);
    _tint_pulling_pos = ((custom_instance_index * 4u) + 0u);
    var_b = bitcast<f32>(_tint_pulling_vertex_buffer_1._tint_vertex_data[(_tint_pulling_pos / 4u)]);
  }
}
)";

  VertexPulling::Config cfg;
  cfg.vertex_state = {{
      {
          4,
          InputStepMode::kVertex,
          {{VertexFormat::kF32, 0, 0}},
      },
      {
          4,
          InputStepMode::kInstance,
          {{VertexFormat::kF32, 0, 1}},
      },
  }};
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, TwoAttributesSameBuffer) {
  auto* src = R"(
[[location(0)]] var<in> var_a : f32;
[[location(1)]] var<in> var_b : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[builtin(vertex_index)]] var<in> _tint_pulling_vertex_index : u32;

[[binding(0), group(4)]] var<storage> _tint_pulling_vertex_buffer_0 : TintVertexData;

[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

var<private> var_a : f32;

var<private> var_b : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 16u) + 0u);
    var_a = bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[(_tint_pulling_pos / 4u)]);
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 16u) + 0u);
    var_b = vec4<f32>(bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[((_tint_pulling_pos + 0u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[((_tint_pulling_pos + 4u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[((_tint_pulling_pos + 8u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[((_tint_pulling_pos + 12u) / 4u)]));
  }
}
)";

  VertexPulling::Config cfg;
  cfg.vertex_state = {
      {{16,
        InputStepMode::kVertex,
        {{VertexFormat::kF32, 0, 0}, {VertexFormat::kVec4F32, 0, 1}}}}};
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

TEST_F(VertexPullingTest, FloatVectorAttributes) {
  auto* src = R"(
[[location(0)]] var<in> var_a : vec2<f32>;
[[location(1)]] var<in> var_b : vec3<f32>;
[[location(2)]] var<in> var_c : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {}
)";

  auto* expect = R"(
[[builtin(vertex_index)]] var<in> _tint_pulling_vertex_index : u32;

[[binding(0), group(4)]] var<storage> _tint_pulling_vertex_buffer_0 : TintVertexData;

[[binding(1), group(4)]] var<storage> _tint_pulling_vertex_buffer_1 : TintVertexData;

[[binding(2), group(4)]] var<storage> _tint_pulling_vertex_buffer_2 : TintVertexData;

[[block]]
struct TintVertexData {
  _tint_vertex_data : [[stride(4)]] array<u32>;
};

var<private> var_a : vec2<f32>;

var<private> var_b : vec3<f32>;

var<private> var_c : vec4<f32>;

[[stage(vertex)]]
fn main() -> void {
  {
    var _tint_pulling_pos : u32;
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 8u) + 0u);
    var_a = vec2<f32>(bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[((_tint_pulling_pos + 0u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_0._tint_vertex_data[((_tint_pulling_pos + 4u) / 4u)]));
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 12u) + 0u);
    var_b = vec3<f32>(bitcast<f32>(_tint_pulling_vertex_buffer_1._tint_vertex_data[((_tint_pulling_pos + 0u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_1._tint_vertex_data[((_tint_pulling_pos + 4u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_1._tint_vertex_data[((_tint_pulling_pos + 8u) / 4u)]));
    _tint_pulling_pos = ((_tint_pulling_vertex_index * 16u) + 0u);
    var_c = vec4<f32>(bitcast<f32>(_tint_pulling_vertex_buffer_2._tint_vertex_data[((_tint_pulling_pos + 0u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_2._tint_vertex_data[((_tint_pulling_pos + 4u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_2._tint_vertex_data[((_tint_pulling_pos + 8u) / 4u)]), bitcast<f32>(_tint_pulling_vertex_buffer_2._tint_vertex_data[((_tint_pulling_pos + 12u) / 4u)]));
  }
}
)";

  VertexPulling::Config cfg;
  cfg.vertex_state = {{
      {8, InputStepMode::kVertex, {{VertexFormat::kVec2F32, 0, 0}}},
      {12, InputStepMode::kVertex, {{VertexFormat::kVec3F32, 0, 1}}},
      {16, InputStepMode::kVertex, {{VertexFormat::kVec4F32, 0, 2}}},
  }};
  cfg.entry_point_name = "main";

  auto transform = std::make_unique<VertexPulling>(cfg);

  auto got = Run(src, std::move(transform));

  EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace transform
}  // namespace tint
