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

#include "src/ast/constant_id_decoration.h"

namespace tint {
namespace ast {

ConstantIdDecoration::ConstantIdDecoration(uint32_t val, const Source& source)
    : VariableDecoration(source), value_(val) {}

ConstantIdDecoration::~ConstantIdDecoration() = default;

bool ConstantIdDecoration::IsConstantId() const {
  return true;
}

void ConstantIdDecoration::to_str(std::ostream& out) const {
  out << "ConstantIdDecoration{" << value_ << "}" << std::endl;
}

}  // namespace ast
}  // namespace tint
