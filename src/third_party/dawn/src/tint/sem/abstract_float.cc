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

#include "src/tint/sem/abstract_float.h"

#include "src/tint/program_builder.h"
#include "src/tint/utils/hash.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::AbstractFloat);

namespace tint::sem {

AbstractFloat::AbstractFloat() = default;
AbstractFloat::AbstractFloat(AbstractFloat&&) = default;
AbstractFloat::~AbstractFloat() = default;

size_t AbstractFloat::Hash() const {
    return utils::Hash(TypeInfo::Of<AbstractFloat>().full_hashcode);
}

bool AbstractFloat::Equals(const sem::Type& other) const {
    return other.Is<AbstractFloat>();
}

std::string AbstractFloat::FriendlyName(const SymbolTable&) const {
    return "abstract-float";
}

}  // namespace tint::sem
