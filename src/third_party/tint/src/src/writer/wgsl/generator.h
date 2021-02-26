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

#ifndef SRC_WRITER_WGSL_GENERATOR_H_
#define SRC_WRITER_WGSL_GENERATOR_H_

#include <memory>
#include <string>

#include "src/writer/text.h"
#include "src/writer/wgsl/generator_impl.h"

namespace tint {
namespace writer {
namespace wgsl {

/// Class to generate WGSL source
class Generator : public Text {
 public:
  /// Constructor
  /// @param module the module to convert
  explicit Generator(ast::Module module);
  ~Generator() override;

  /// Resets the generator
  void Reset() override;

  /// Generates the result data
  /// @returns true on successful generation; false otherwise
  bool Generate() override;

  /// Converts a single entry point
  /// @param stage the pipeline stage
  /// @param name the entry point name
  /// @returns true on succes; false on failure
  bool GenerateEntryPoint(ast::PipelineStage stage,
                          const std::string& name) override;

  /// @returns the result data
  std::string result() const override;

  /// @returns the error
  std::string error() const;

 private:
  std::unique_ptr<GeneratorImpl> impl_;
};

}  // namespace wgsl
}  // namespace writer
}  // namespace tint

#endif  // SRC_WRITER_WGSL_GENERATOR_H_
