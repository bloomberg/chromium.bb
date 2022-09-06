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

#ifndef SRC_TINT_SEM_TYPE_H_
#define SRC_TINT_SEM_TYPE_H_

#include <functional>
#include <string>

#include "src/tint/sem/node.h"

// Forward declarations
namespace tint {
class ProgramBuilder;
class SymbolTable;
}  // namespace tint

namespace tint::sem {

/// Supported memory layouts for calculating sizes
enum class MemoryLayout { kUniformBuffer, kStorageBuffer };

/// Base class for a type in the system
class Type : public Castable<Type, Node> {
  public:
    /// Move constructor
    Type(Type&&);
    ~Type() override;

    /// @returns a hash of the type.
    virtual size_t Hash() const = 0;

    /// @returns true if the this type is equal to the given type
    virtual bool Equals(const Type&) const = 0;

    /// @param symbols the program's symbol table
    /// @returns the name for this type that closely resembles how it would be
    /// declared in WGSL.
    virtual std::string FriendlyName(const SymbolTable& symbols) const = 0;

    /// @returns the inner most pointee type if this is a pointer, `this`
    /// otherwise
    const Type* UnwrapPtr() const;

    /// @returns the inner type if this is a reference, `this` otherwise
    const Type* UnwrapRef() const;

    /// @returns the size in bytes of the type. This may include tail padding.
    /// @note opaque types will return a size of 0.
    virtual uint32_t Size() const;

    /// @returns the alignment in bytes of the type. This may include tail
    /// padding.
    /// @note opaque types will return a size of 0.
    virtual uint32_t Align() const;

    /// @returns true if constructible as per
    /// https://gpuweb.github.io/gpuweb/wgsl/#constructible-types
    virtual bool IsConstructible() const;

    /// @returns true if this type is a scalar
    bool is_scalar() const;
    /// @returns true if this type is a scalar or an abstract numeric
    bool is_abstract_or_scalar() const;
    /// @returns true if this type is a numeric scalar
    bool is_numeric_scalar() const;
    /// @returns true if this type is a float scalar
    bool is_float_scalar() const;
    /// @returns true if this type is a float matrix
    bool is_float_matrix() const;
    /// @returns true if this type is a square float matrix
    bool is_square_float_matrix() const;
    /// @returns true if this type is a float vector
    bool is_float_vector() const;
    /// @returns true if this type is a float scalar or vector
    bool is_float_scalar_or_vector() const;
    /// @returns true if this type is a float scalar or vector or matrix
    bool is_float_scalar_or_vector_or_matrix() const;
    /// @returns true if this type is an integer scalar
    bool is_integer_scalar() const;
    /// @returns true if this type is a signed integer scalar
    bool is_signed_integer_scalar() const;
    /// @returns true if this type is an unsigned integer scalar
    bool is_unsigned_integer_scalar() const;
    /// @returns true if this type is a signed integer vector
    bool is_signed_integer_vector() const;
    /// @returns true if this type is an unsigned vector
    bool is_unsigned_integer_vector() const;
    /// @returns true if this type is an unsigned scalar or vector
    bool is_unsigned_scalar_or_vector() const;
    /// @returns true if this type is a signed scalar or vector
    bool is_signed_scalar_or_vector() const;
    /// @returns true if this type is an integer scalar or vector
    bool is_integer_scalar_or_vector() const;
    /// @returns true if this type is a boolean vector
    bool is_bool_vector() const;
    /// @returns true if this type is boolean scalar or vector
    bool is_bool_scalar_or_vector() const;
    /// @returns true if this type is a numeric vector
    bool is_numeric_vector() const;
    /// @returns true if this type is a vector of scalar type
    bool is_scalar_vector() const;
    /// @returns true if this type is a numeric scale or vector
    bool is_numeric_scalar_or_vector() const;
    /// @returns true if this type is a handle type
    bool is_handle() const;

    /// kNoConversion is returned from ConversionRank() when the implicit conversion is not
    /// permitted.
    static constexpr uint32_t kNoConversion = 0xffffffffu;

    /// ConversionRank returns the implicit conversion rank when attempting to convert `from` to
    /// `to`. Lower ranks are preferred over higher ranks.
    /// @param from the source type
    /// @param to the destination type
    /// @returns the rank value for converting from type `from` to type `to`, or #kNoConversion if
    /// the implicit conversion is not allowed.
    /// @see https://www.w3.org/TR/WGSL/#conversion-rank
    static uint32_t ConversionRank(const Type* from, const Type* to);

    /// @param ty the type to obtain the element type from
    /// @param count if not null, then this is assigned the number of elements in the type
    /// @returns `ty` if `ty` is an abstract or scalar, the element type if ty is a vector, matrix
    /// or array, otherwise nullptr.
    static const Type* ElementOf(const Type* ty, uint32_t* count = nullptr);

    /// @param types a pointer to a list of `const Type*`.
    /// @param count the number of types in `types`.
    /// @returns the lowest-ranking type that all types in `types` can be implicitly converted to,
    /// or nullptr if there is no consistent common type across all types in `types`.
    /// @see https://www.w3.org/TR/WGSL/#conversion-rank
    static const sem::Type* Common(Type const* const* types, size_t count);

    /// @param types an initializer_list of `const Type*`.
    /// @returns the lowest-ranking type that all types in `types` can be implicitly converted to,
    /// or nullptr if there is no consistent common type across all types in `types`.
    /// @see https://www.w3.org/TR/WGSL/#conversion-rank
    static const sem::Type* Common(std::initializer_list<const Type*> types) {
        return Common(types.begin(), types.size());
    }

  protected:
    Type();
};

}  // namespace tint::sem

namespace std {

/// std::hash specialization for tint::sem::Type
template <>
struct hash<tint::sem::Type> {
    /// @param type the type to obtain a hash from
    /// @returns the hash of the semantic type
    size_t operator()(const tint::sem::Type& type) const { return type.Hash(); }
};

/// std::equal_to specialization for tint::sem::Type
template <>
struct equal_to<tint::sem::Type> {
    /// @param a the first type to compare
    /// @param b the second type to compare
    /// @returns true if the two types are equal
    bool operator()(const tint::sem::Type& a, const tint::sem::Type& b) const {
        return a.Equals(b);
    }
};

}  // namespace std

#endif  // SRC_TINT_SEM_TYPE_H_
