// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0(the "License");
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

#include "src/tint/sem/call.h"

#include <utility>
#include <vector>

TINT_INSTANTIATE_TYPEINFO(tint::sem::Call);

namespace tint::sem {

Call::Call(const ast::CallExpression* declaration,
           const CallTarget* target,
           std::vector<const sem::Expression*> arguments,
           const Statement* statement,
           Constant constant,
           bool has_side_effects)
    : Base(declaration, target->ReturnType(), statement, std::move(constant), has_side_effects),
      target_(target),
      arguments_(std::move(arguments)) {}

Call::~Call() = default;

}  // namespace tint::sem
