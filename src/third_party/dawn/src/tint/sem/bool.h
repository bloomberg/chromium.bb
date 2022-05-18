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

#ifndef SRC_TINT_SEM_BOOL_H_
#define SRC_TINT_SEM_BOOL_H_

#include <string>

#include "src/tint/sem/type.h"

// X11 likes to #define Bool leading to confusing error messages.
// If its defined, undefine it.
#ifdef Bool
#undef Bool
#endif

namespace tint::sem {

/// A boolean type
class Bool final : public Castable<Bool, Type> {
  public:
    /// Constructor
    Bool();
    /// Move constructor
    Bool(Bool&&);
    ~Bool() override;

    /// @returns a hash of the type.
    size_t Hash() const override;

    /// @param other the other type to compare against
    /// @returns true if the this type is equal to the given type
    bool Equals(const Type& other) const override;

    /// @param symbols the program's symbol table
    /// @returns the name for this type that closely resembles how it would be
    /// declared in WGSL.
    std::string FriendlyName(const SymbolTable& symbols) const override;

    /// @returns true if constructible as per
    /// https://gpuweb.github.io/gpuweb/wgsl/#constructible-types
    bool IsConstructible() const override;

    /// @returns the size in bytes of the type.
    /// @note: booleans are not host-sharable, but still may exist in workgroup
    /// storage.
    uint32_t Size() const override;

    /// @returns the alignment in bytes of the type.
    /// @note: booleans are not host-sharable, but still may exist in workgroup
    /// storage.
    uint32_t Align() const override;
};

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_BOOL_H_
