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

#ifndef SRC_TYPE_TYPE_H_
#define SRC_TYPE_TYPE_H_

#include <string>

#include "src/clone_context.h"

namespace tint {

// Forward declarations
class ProgramBuilder;
class SymbolTable;

namespace type {

/// Supported memory layouts for calculating sizes
enum class MemoryLayout { kUniformBuffer, kStorageBuffer };

/// Base class for a type in the system
class Type : public Castable<Type, Cloneable> {
 public:
  /// Move constructor
  Type(Type&&);
  ~Type() override;

  /// @returns the name for this type. The type name is unique over all types.
  virtual std::string type_name() const = 0;

  /// @param symbols the program's symbol table
  /// @returns the name for this type that closely resembles how it would be
  /// declared in WGSL.
  virtual std::string FriendlyName(const SymbolTable& symbols) const = 0;

  /// @returns the pointee type if this is a pointer, `this` otherwise
  Type* UnwrapPtrIfNeeded();

  /// @returns the most deeply nested aliased type if this is an alias, `this`
  /// otherwise
  Type* UnwrapAliasIfNeeded();

  /// Removes all levels of aliasing and access control.
  /// This is just enough to assist with WGSL translation
  /// in that you want see through one level of pointer to get from an
  /// identifier-like expression as an l-value to its corresponding r-value,
  /// plus see through the wrappers on either side.
  /// @returns the completely unaliased type.
  Type* UnwrapIfNeeded();

  /// Returns the type found after:
  /// - removing all layers of aliasing and access control if they exist, then
  /// - removing the pointer, if it exists, then
  /// - removing all further layers of aliasing or access control, if they exist
  /// @returns the unwrapped type
  Type* UnwrapAll();

  /// @returns true if this type is a scalar
  bool is_scalar() const;
  /// @returns true if this type is a float scalar
  bool is_float_scalar() const;
  /// @returns true if this type is a float matrix
  bool is_float_matrix() const;
  /// @returns true if this type is a float vector
  bool is_float_vector() const;
  /// @returns true if this type is a float scalar or vector
  bool is_float_scalar_or_vector() const;
  /// @returns true if this type is a float scalar or vector or matrix
  bool is_float_scalar_or_vector_or_matrix() const;
  /// @returns true if this type is an integer scalar
  bool is_integer_scalar() const;
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
  /// @returns true if this type is a handle type
  bool is_handle() const;

 protected:
  Type();
};

}  // namespace type
}  // namespace tint

#endif  // SRC_TYPE_TYPE_H_
