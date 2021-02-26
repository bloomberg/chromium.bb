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

#include "src/ast/type/f32_type.h"

namespace tint {
namespace ast {
namespace type {

F32Type::F32Type() = default;

F32Type::~F32Type() = default;

bool F32Type::IsF32() const {
  return true;
}

std::string F32Type::type_name() const {
  return "__f32";
}

uint64_t F32Type::MinBufferBindingSize(MemoryLayout) const {
  return 4;
}

uint64_t F32Type::BaseAlignment(MemoryLayout) const {
  return 4;
}

}  // namespace type
}  // namespace ast
}  // namespace tint
