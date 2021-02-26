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

#ifndef SRC_AST_TYPE_ARRAY_TYPE_H_
#define SRC_AST_TYPE_ARRAY_TYPE_H_

#include <assert.h>

#include <string>
#include <utility>

#include "src/ast/array_decoration.h"
#include "src/ast/type/type.h"

namespace tint {
namespace ast {
namespace type {

/// An array type. If size is zero then it is a runtime array.
class ArrayType : public Type {
 public:
  /// Constructor for runtime array
  /// @param subtype the type of the array elements
  explicit ArrayType(Type* subtype);
  /// Constructor
  /// @param subtype the type of the array elements
  /// @param size the number of elements in the array
  ArrayType(Type* subtype, uint32_t size);
  /// Move constructor
  ArrayType(ArrayType&&);
  ~ArrayType() override;

  /// @returns true if the type is an array type
  bool IsArray() const override;
  /// @returns true if this is a runtime array.
  /// i.e. the size is determined at runtime
  bool IsRuntimeArray() const { return size_ == 0; }

  /// @param mem_layout type of memory layout to use in calculation.
  /// @returns minimum size required for this type, in bytes.
  ///          0 for non-host shareable types.
  uint64_t MinBufferBindingSize(MemoryLayout mem_layout) const override;

  /// @param mem_layout type of memory layout to use in calculation.
  /// @returns base alignment for the type, in bytes.
  ///          0 for non-host shareable types.
  uint64_t BaseAlignment(MemoryLayout mem_layout) const override;

  /// Sets the array decorations
  /// @param decos the decorations to set
  void set_decorations(ast::ArrayDecorationList decos) {
    decos_ = std::move(decos);
  }
  /// @returns the array decorations
  const ArrayDecorationList& decorations() const { return decos_; }

  /// @returns the array stride or 0 if none set.
  uint32_t array_stride() const;
  /// @returns true if the array has a stride set
  bool has_array_stride() const;

  /// @returns the array type
  Type* type() const { return subtype_; }
  /// @returns the array size. Size is 0 for a runtime array
  uint32_t size() const { return size_; }

  /// @returns the name for the type
  std::string type_name() const override;

 private:
  Type* subtype_ = nullptr;
  uint32_t size_ = 0;
  ast::ArrayDecorationList decos_;
};

}  // namespace type
}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_TYPE_ARRAY_TYPE_H_
