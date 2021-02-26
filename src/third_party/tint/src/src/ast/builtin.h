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

#ifndef SRC_AST_BUILTIN_H_
#define SRC_AST_BUILTIN_H_

#include <ostream>

namespace tint {
namespace ast {

/// The builtin identifiers
enum class Builtin {
  kNone = -1,
  kPosition,
  kVertexIdx,
  kInstanceIdx,
  kFrontFacing,
  kFragCoord,
  kFragDepth,
  kLocalInvocationId,
  kLocalInvocationIdx,
  kGlobalInvocationId
};

std::ostream& operator<<(std::ostream& out, Builtin builtin);

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_BUILTIN_H_
