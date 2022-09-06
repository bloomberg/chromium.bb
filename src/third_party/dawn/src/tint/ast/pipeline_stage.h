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

#ifndef SRC_TINT_AST_PIPELINE_STAGE_H_
#define SRC_TINT_AST_PIPELINE_STAGE_H_

#include <ostream>

namespace tint::ast {

/// The pipeline stage
enum class PipelineStage { kNone = -1, kVertex, kFragment, kCompute };

/// @param out the std::ostream to write to
/// @param stage the PipelineStage
/// @return the std::ostream so calls can be chained
std::ostream& operator<<(std::ostream& out, PipelineStage stage);

}  // namespace tint::ast

#endif  // SRC_TINT_AST_PIPELINE_STAGE_H_
