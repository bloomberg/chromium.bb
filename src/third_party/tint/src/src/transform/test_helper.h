// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TRANSFORM_TEST_HELPER_H_
#define SRC_TRANSFORM_TEST_HELPER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "src/program.h"
#include "src/program_builder.h"
#include "src/reader/wgsl/parser.h"
#include "src/transform/manager.h"
#include "src/type_determiner.h"
#include "src/writer/wgsl/generator.h"

namespace tint {
namespace transform {

/// Helper class for testing transforms
class TransformTest : public testing::Test {
 public:
  /// Transforms and returns the WGSL source `in`, transformed using
  /// `transforms`.
  /// @param in the input WGSL source
  /// @param transforms the list of transforms to apply
  /// @return the transformed output
  Transform::Output Transform(
      std::string in,
      std::vector<std::unique_ptr<transform::Transform>> transforms) {
    Source::File file("test", in);
    auto program = reader::wgsl::Parse(&file);

    if (!program.IsValid()) {
      return Transform::Output(std::move(program));
    }

    Manager manager;
    for (auto& transform : transforms) {
      manager.append(std::move(transform));
    }
    return manager.Run(&program);
  }

  /// Transforms and returns the WGSL source `in`, transformed using
  /// `transform`.
  /// @param transform the transform to apply
  /// @param in the input WGSL source
  /// @return the transformed output
  Transform::Output Transform(std::string in,
                              std::unique_ptr<transform::Transform> transform) {
    std::vector<std::unique_ptr<transform::Transform>> transforms;
    transforms.emplace_back(std::move(transform));
    return Transform(std::move(in), std::move(transforms));
  }

  /// Transforms and returns the WGSL source `in`, transformed using
  /// a transform of type `TRANSFORM`.
  /// @param in the input WGSL source
  /// @param args the TRANSFORM constructor arguments
  /// @return the transformed output
  template <typename TRANSFORM, typename... ARGS>
  Transform::Output Transform(std::string in, ARGS&&... args) {
    return Transform(std::move(in),
                     std::make_unique<TRANSFORM>(std::forward<ARGS>(args)...));
  }

  /// @param output the output of the transform
  /// @returns the output program as a WGSL string, or an error string if the
  /// program is not valid.
  std::string str(const Transform::Output& output) {
    diag::Formatter::Style style;
    style.print_newline_at_end = false;

    if (!output.program.IsValid()) {
      return diag::Formatter(style).format(output.program.Diagnostics());
    }

    writer::wgsl::Generator generator(&output.program);
    if (!generator.Generate()) {
      return "WGSL writer failed:\n" + generator.error();
    }

    auto res = generator.result();
    if (res.empty()) {
      return res;
    }
    // The WGSL sometimes has two trailing newlines. Strip them
    while (res.back() == '\n') {
      res.pop_back();
    }
    if (res.empty()) {
      return res;
    }
    return "\n" + res + "\n";
  }
};

}  // namespace transform
}  // namespace tint

#endif  // SRC_TRANSFORM_TEST_HELPER_H_
