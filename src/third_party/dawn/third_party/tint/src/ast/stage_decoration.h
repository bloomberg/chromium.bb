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

#include <string>

#include "src/ast/decoration.h"
#include "src/ast/pipeline_stage.h"

namespace tint {
namespace ast {

/// A workgroup decoration
class StageDecoration : public Castable<StageDecoration, Decoration> {
 public:
  /// constructor
  /// @param program_id the identifier of the program that owns this node
  /// @param stage the pipeline stage
  /// @param source the source of this decoration
  StageDecoration(ProgramID program_id,
                  const Source& source,
                  PipelineStage stage);
  ~StageDecoration() override;

  /// @returns the WGSL name for the decoration
  std::string Name() const override;

  /// Clones this node and all transitive child nodes using the `CloneContext`
  /// `ctx`.
  /// @param ctx the clone context
  /// @return the newly cloned node
  const StageDecoration* Clone(CloneContext* ctx) const override;

  /// The pipeline stage
  const PipelineStage stage;
};

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_STAGE_DECORATION_H_
