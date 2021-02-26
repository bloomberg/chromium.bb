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

#ifndef SRC_AST_STAGE_DECORATION_H_
#define SRC_AST_STAGE_DECORATION_H_

#include "src/ast/function_decoration.h"
#include "src/ast/pipeline_stage.h"

namespace tint {
namespace ast {

/// A workgroup decoration
class StageDecoration : public FunctionDecoration {
 public:
  /// constructor
  /// @param stage the pipeline stage
  /// @param source the source of this decoration
  StageDecoration(ast::PipelineStage stage, const Source& source);
  ~StageDecoration() override;

  /// @returns true if this is a stage decoration
  bool IsStage() const override;

  /// @returns the stage
  ast::PipelineStage value() const { return stage_; }

  /// Outputs the decoration to the given stream
  /// @param out the stream to output too
  void to_str(std::ostream& out) const override;

 private:
  ast::PipelineStage stage_ = ast::PipelineStage::kNone;
};

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_STAGE_DECORATION_H_
