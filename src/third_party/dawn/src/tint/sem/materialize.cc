// Copyright 2022 The Tint Authors.
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

#include "src/tint/sem/materialize.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::Materialize);

namespace tint::sem {

Materialize::Materialize(const Expression* expr, const Statement* statement, Constant constant)
    : Base(/* declaration */ expr->Declaration(),
           /* type */ constant.Type(),
           /* statement */ statement,
           /* constant */ constant,
           /* has_side_effects */ false,
           /* source_var */ expr->SourceVariable()),
      expr_(expr) {
    // Materialize nodes only wrap compile-time expressions, and so the Materialize expression must
    // have a constant value.
    TINT_ASSERT(Semantic, constant.IsValid());
}

Materialize::~Materialize() = default;

}  // namespace tint::sem
