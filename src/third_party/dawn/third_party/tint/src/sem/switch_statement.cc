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

#include "src/sem/switch_statement.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::SwitchStatement);
TINT_INSTANTIATE_TYPEINFO(tint::sem::SwitchCaseBlockStatement);

namespace tint {
namespace sem {

SwitchStatement::SwitchStatement(const ast::SwitchStatement* declaration,
                                 CompoundStatement* parent)
    : Base(declaration, parent) {}

SwitchStatement::~SwitchStatement() = default;

SwitchCaseBlockStatement::SwitchCaseBlockStatement(
    const ast::BlockStatement* declaration,
    const CompoundStatement* parent)
    : Base(declaration, parent) {
  TINT_ASSERT(Semantic, parent);
}
SwitchCaseBlockStatement::~SwitchCaseBlockStatement() = default;

}  // namespace sem
}  // namespace tint
