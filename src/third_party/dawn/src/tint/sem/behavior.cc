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

#include "src/tint/sem/behavior.h"

namespace tint::sem {

std::ostream& operator<<(std::ostream& out, Behavior behavior) {
    switch (behavior) {
        case Behavior::kReturn:
            return out << "Return";
        case Behavior::kDiscard:
            return out << "Discard";
        case Behavior::kBreak:
            return out << "Break";
        case Behavior::kContinue:
            return out << "Continue";
        case Behavior::kFallthrough:
            return out << "Fallthrough";
        case Behavior::kNext:
            return out << "Next";
    }
    return out << "<unknown>";
}

}  // namespace tint::sem
