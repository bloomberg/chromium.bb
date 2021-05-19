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

#include "src/ast/bool_literal.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::BoolLiteral);

namespace tint {
namespace ast {

BoolLiteral::BoolLiteral(const Source& source, type::Type* type, bool value)
    : Base(source, type), value_(value) {}

BoolLiteral::~BoolLiteral() = default;

std::string BoolLiteral::to_str(const semantic::Info&) const {
  return value_ ? "true" : "false";
}

std::string BoolLiteral::name() const {
  return value_ ? "__bool_true" : "__bool_false";
}

BoolLiteral* BoolLiteral::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  auto* ty = ctx->Clone(type());
  return ctx->dst->create<BoolLiteral>(src, ty, value_);
}

}  // namespace ast
}  // namespace tint
