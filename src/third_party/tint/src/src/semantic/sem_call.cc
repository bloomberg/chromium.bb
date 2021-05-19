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

#include "src/semantic/call.h"

TINT_INSTANTIATE_TYPEINFO(tint::semantic::Call);

namespace tint {
namespace semantic {

Call::Call(ast::Expression* declaration,
           const CallTarget* target,
           Statement* statement)
    : Base(declaration, target->ReturnType(), statement), target_(target) {}

Call::~Call() = default;

}  // namespace semantic
}  // namespace tint
