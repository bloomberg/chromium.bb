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

#ifndef SRC_TINT_WRITER_SPIRV_SCALAR_CONSTANT_H_
#define SRC_TINT_WRITER_SPIRV_SCALAR_CONSTANT_H_

#include <stdint.h>

#include <cstring>
#include <functional>

#include "src/tint/utils/hash.h"

// Forward declarations
namespace tint::sem {
class Call;
}  // namespace tint::sem

namespace tint::writer::spirv {

/// ScalarConstant represents a scalar constant value
struct ScalarConstant {
  /// The constant value
  union Value {
    /// The value as a bool
    bool b;
    /// The value as a uint32_t
    uint32_t u32;
    /// The value as a int32_t
    int32_t i32;
    /// The value as a float
    float f32;

    /// The value that is wide enough to encompass all other types (including
    /// future 64-bit data types).
    uint64_t u64;
  };

  /// The kind of constant
  enum class Kind { kBool, kU32, kI32, kF32 };

  /// Constructor
  inline ScalarConstant() { value.u64 = 0; }

  /// @param value the value of the constant
  /// @returns a new ScalarConstant with the provided value and kind Kind::kU32
  static inline ScalarConstant U32(uint32_t value) {
    ScalarConstant c;
    c.value.u32 = value;
    c.kind = Kind::kU32;
    return c;
  }

  /// @param value the value of the constant
  /// @returns a new ScalarConstant with the provided value and kind Kind::kI32
  static inline ScalarConstant I32(int32_t value) {
    ScalarConstant c;
    c.value.i32 = value;
    c.kind = Kind::kI32;
    return c;
  }

  /// @param value the value of the constant
  /// @returns a new ScalarConstant with the provided value and kind Kind::kI32
  static inline ScalarConstant F32(float value) {
    ScalarConstant c;
    c.value.f32 = value;
    c.kind = Kind::kF32;
    return c;
  }

  /// @param value the value of the constant
  /// @returns a new ScalarConstant with the provided value and kind Kind::kBool
  static inline ScalarConstant Bool(bool value) {
    ScalarConstant c;
    c.value.b = value;
    c.kind = Kind::kBool;
    return c;
  }

  /// Equality operator
  /// @param rhs the ScalarConstant to compare against
  /// @returns true if this ScalarConstant is equal to `rhs`
  inline bool operator==(const ScalarConstant& rhs) const {
    return value.u64 == rhs.value.u64 && kind == rhs.kind &&
           is_spec_op == rhs.is_spec_op && constant_id == rhs.constant_id;
  }

  /// Inequality operator
  /// @param rhs the ScalarConstant to compare against
  /// @returns true if this ScalarConstant is not equal to `rhs`
  inline bool operator!=(const ScalarConstant& rhs) const {
    return !(*this == rhs);
  }

  /// @returns this ScalarConstant as a specialization op with the given
  /// specialization constant identifier
  /// @param id the constant identifier
  ScalarConstant AsSpecOp(uint32_t id) const {
    auto ret = *this;
    ret.is_spec_op = true;
    ret.constant_id = id;
    return ret;
  }

  /// The constant value
  Value value;
  /// The constant value kind
  Kind kind = Kind::kBool;
  /// True if the constant is a specialization op
  bool is_spec_op = false;
  /// The identifier if a specialization op
  uint32_t constant_id = 0;
};

}  // namespace tint::writer::spirv

namespace std {

/// Custom std::hash specialization for tint::Symbol so symbols can be used as
/// keys for std::unordered_map and std::unordered_set.
template <>
class hash<tint::writer::spirv::ScalarConstant> {
 public:
  /// @param c the ScalarConstant
  /// @return the Symbol internal value
  inline std::size_t operator()(
      const tint::writer::spirv::ScalarConstant& c) const {
    uint32_t value = 0;
    std::memcpy(&value, &c.value, sizeof(value));
    return tint::utils::Hash(value, c.kind);
  }
};

}  // namespace std

#endif  // SRC_TINT_WRITER_SPIRV_SCALAR_CONSTANT_H_
