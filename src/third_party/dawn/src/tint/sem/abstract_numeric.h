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

#ifndef SRC_TINT_SEM_ABSTRACT_NUMERIC_H_
#define SRC_TINT_SEM_ABSTRACT_NUMERIC_H_

#include <string>

#include "src/tint/sem/type.h"

namespace tint::sem {

/// The base class for abstract-int and abstract-float types.
/// @see https://www.w3.org/TR/WGSL/#types-for-creation-time-constants
class AbstractNumeric : public Castable<AbstractNumeric, Type> {
  public:
    /// Constructor
    AbstractNumeric();

    /// Move constructor
    AbstractNumeric(AbstractNumeric&&);
    ~AbstractNumeric() override;

    /// @returns 0, as the type is abstract.
    uint32_t Size() const override;

    /// @returns 0, as the type is abstract.
    uint32_t Align() const override;

    /// @returns 0, as the type is abstract.
    bool IsConstructible() const override;
};

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_ABSTRACT_NUMERIC_H_
