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

#ifndef SRC_TINT_AST_LOCATION_ATTRIBUTE_H_
#define SRC_TINT_AST_LOCATION_ATTRIBUTE_H_

#include <string>

#include "src/tint/ast/attribute.h"

namespace tint::ast {

/// A location attribute
class LocationAttribute final : public Castable<LocationAttribute, Attribute> {
  public:
    /// constructor
    /// @param pid the identifier of the program that owns this node
    /// @param src the source of this node
    /// @param value the location value
    LocationAttribute(ProgramID pid, const Source& src, uint32_t value);
    ~LocationAttribute() override;

    /// @returns the WGSL name for the attribute
    std::string Name() const override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const LocationAttribute* Clone(CloneContext* ctx) const override;

    /// The location value
    const uint32_t value;
};

}  // namespace tint::ast

#endif  // SRC_TINT_AST_LOCATION_ATTRIBUTE_H_
