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

#include "src/tint/ast/u32.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::U32);

namespace tint::ast {

U32::U32(ProgramID pid, const Source& src) : Base(pid, src) {}

U32::~U32() = default;

U32::U32(U32&&) = default;

std::string U32::FriendlyName(const SymbolTable&) const {
    return "u32";
}

const U32* U32::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    return ctx->dst->create<U32>(src);
}

}  // namespace tint::ast
