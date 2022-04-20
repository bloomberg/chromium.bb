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

#include "src/tint/sem/call_target.h"

#include "src/tint/symbol_table.h"
#include "src/tint/utils/hash.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::CallTarget);

namespace tint::sem {

CallTarget::CallTarget(const sem::Type* return_type,
                       const ParameterList& parameters)
    : signature_{return_type, parameters} {
  TINT_ASSERT(Semantic, return_type);
}

CallTarget::CallTarget(const CallTarget&) = default;
CallTarget::~CallTarget() = default;

CallTargetSignature::CallTargetSignature(const sem::Type* ret_ty,
                                         const ParameterList& params)
    : return_type(ret_ty), parameters(params) {}
CallTargetSignature::CallTargetSignature(const CallTargetSignature&) = default;
CallTargetSignature::~CallTargetSignature() = default;

int CallTargetSignature::IndexOf(ParameterUsage usage) const {
  for (size_t i = 0; i < parameters.size(); i++) {
    if (parameters[i]->Usage() == usage) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool CallTargetSignature::operator==(const CallTargetSignature& other) const {
  if (return_type != other.return_type ||
      parameters.size() != other.parameters.size()) {
    return false;
  }
  for (size_t i = 0; i < parameters.size(); i++) {
    auto* a = parameters[i];
    auto* b = other.parameters[i];
    if (a->Type() != b->Type() || a->Usage() != b->Usage()) {
      return false;
    }
  }
  return true;
}

}  // namespace tint::sem

namespace std {

std::size_t hash<tint::sem::CallTargetSignature>::operator()(
    const tint::sem::CallTargetSignature& sig) const {
  size_t hash = tint::utils::Hash(sig.parameters.size());
  for (auto* p : sig.parameters) {
    tint::utils::HashCombine(&hash, p->Type(), p->Usage());
  }
  return tint::utils::Hash(hash, sig.return_type);
}

}  // namespace std
