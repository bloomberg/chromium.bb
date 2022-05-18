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

#ifndef SRC_TINT_SEM_BEHAVIOR_H_
#define SRC_TINT_SEM_BEHAVIOR_H_

#include "src/tint/utils/enum_set.h"

namespace tint::sem {

/// Behavior enumerates the possible behaviors of an expression or statement.
/// @see https://www.w3.org/TR/WGSL/#behaviors
enum class Behavior {
    kReturn,
    kDiscard,
    kBreak,
    kContinue,
    kFallthrough,
    kNext,
};

/// Behaviors is a set of Behavior
using Behaviors = utils::EnumSet<Behavior>;

/// Writes the Behavior to the std::ostream.
/// @param out the std::ostream to write to
/// @param behavior the Behavior to write
/// @returns out so calls can be chained
std::ostream& operator<<(std::ostream& out, Behavior behavior);

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_BEHAVIOR_H_
