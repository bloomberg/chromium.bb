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

#ifndef SRC_TINT_SEM_STORAGE_TEXTURE_TYPE_H_
#define SRC_TINT_SEM_STORAGE_TEXTURE_TYPE_H_

#include <string>

#include "src/tint/ast/access.h"
#include "src/tint/ast/storage_texture.h"
#include "src/tint/sem/texture_type.h"

// Forward declarations
namespace tint::sem {
class Manager;
}  // namespace tint::sem

namespace tint::sem {

/// A storage texture type.
class StorageTexture final : public Castable<StorageTexture, Texture> {
 public:
  /// Constructor
  /// @param dim the dimensionality of the texture
  /// @param format the texel format of the texture
  /// @param access the access control type of the texture
  /// @param subtype the storage subtype. Use SubtypeFor() to calculate this.
  StorageTexture(ast::TextureDimension dim,
                 ast::TexelFormat format,
                 ast::Access access,
                 sem::Type* subtype);

  /// Move constructor
  StorageTexture(StorageTexture&&);
  ~StorageTexture() override;

  /// @returns a hash of the type.
  size_t Hash() const override;

  /// @param other the other type to compare against
  /// @returns true if the this type is equal to the given type
  bool Equals(const Type& other) const override;

  /// @returns the storage subtype
  Type* type() const { return subtype_; }

  /// @returns the texel format
  ast::TexelFormat texel_format() const { return texel_format_; }

  /// @returns the access control
  ast::Access access() const { return access_; }

  /// @param symbols the program's symbol table
  /// @returns the name for this type that closely resembles how it would be
  /// declared in WGSL.
  std::string FriendlyName(const SymbolTable& symbols) const override;

  /// @param format the storage texture image format
  /// @param type_mgr the sem::Manager used to build the returned type
  /// @returns the storage texture subtype for the given TexelFormat
  static sem::Type* SubtypeFor(ast::TexelFormat format, sem::Manager& type_mgr);

 private:
  ast::TexelFormat const texel_format_;
  ast::Access const access_;
  Type* const subtype_;
};

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_STORAGE_TEXTURE_TYPE_H_
