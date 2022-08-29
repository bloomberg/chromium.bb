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

#include "src/tint/sem/void.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::Void);

namespace tint::sem {

Void::Void() = default;

Void::Void(Void&&) = default;

Void::~Void() = default;

size_t Void::Hash() const {
    return static_cast<size_t>(TypeInfo::Of<Void>().full_hashcode);
}

bool Void::Equals(const Type& other) const {
    return other.Is<Void>();
}

std::string Void::FriendlyName(const SymbolTable&) const {
    return "void";
}

}  // namespace tint::sem
