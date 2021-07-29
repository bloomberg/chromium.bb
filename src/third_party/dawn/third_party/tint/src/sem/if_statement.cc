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

#include "src/sem/if_statement.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::IfStatement);
TINT_INSTANTIATE_TYPEINFO(tint::sem::ElseStatement);

namespace tint {
namespace sem {

IfStatement::IfStatement(const ast::IfStatement* declaration,
                         CompoundStatement* parent)
    : Base(declaration, parent) {}

IfStatement::~IfStatement() = default;

ElseStatement::ElseStatement(const ast::ElseStatement* declaration,
                             CompoundStatement* parent)
    : Base(declaration, parent) {}

ElseStatement::~ElseStatement() = default;

}  // namespace sem
}  // namespace tint
