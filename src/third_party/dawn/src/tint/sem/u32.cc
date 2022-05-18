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

#include "src/tint/sem/u32.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::U32);

namespace tint::sem {

U32::U32() = default;

U32::~U32() = default;

U32::U32(U32&&) = default;

size_t U32::Hash() const {
    return static_cast<size_t>(TypeInfo::Of<U32>().full_hashcode);
}

bool U32::Equals(const Type& other) const {
    return other.Is<U32>();
}

std::string U32::FriendlyName(const SymbolTable&) const {
    return "u32";
}

bool U32::IsConstructible() const {
    return true;
}

uint32_t U32::Size() const {
    return 4;
}

uint32_t U32::Align() const {
    return 4;
}

}  // namespace tint::sem
