// Copyright 2022 The Tint Authors.
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

#include "src/tint/fuzzers/shuffle_transform.h"

#include <random>

#include "src/tint/program_builder.h"

namespace tint::fuzzers {

ShuffleTransform::ShuffleTransform(size_t seed) : seed_(seed) {}

void ShuffleTransform::Run(CloneContext& ctx,
                           const tint::transform::DataMap&,
                           tint::transform::DataMap&) const {
  auto decls = ctx.src->AST().GlobalDeclarations();
  auto rng = std::mt19937_64{seed_};
  std::shuffle(std::begin(decls), std::end(decls), rng);
  for (auto* decl : decls) {
    ctx.dst->AST().AddGlobalDeclaration(ctx.Clone(decl));
  }
}

}  // namespace tint::fuzzers
