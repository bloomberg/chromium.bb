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

#include "src/ast/sint_literal.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::SintLiteral);

namespace tint {
namespace ast {

SintLiteral::SintLiteral(const Source& source, type::Type* type, int32_t value)
    : Base(source, type, static_cast<uint32_t>(value)) {}

SintLiteral::~SintLiteral() = default;

std::string SintLiteral::to_str(const semantic::Info&) const {
  return std::to_string(value());
}

std::string SintLiteral::name() const {
  return "__sint" + type()->type_name() + "_" + std::to_string(value());
}

SintLiteral* SintLiteral::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  auto* ty = ctx->Clone(type());
  return ctx->dst->create<SintLiteral>(src, ty, value());
}

}  // namespace ast
}  // namespace tint
