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

#ifndef SRC_TINT_SEM_ARRAY_H_
#define SRC_TINT_SEM_ARRAY_H_

#include <stdint.h>
#include <string>

#include "src/tint/sem/node.h"
#include "src/tint/sem/type.h"

// Forward declarations
namespace tint::ast {
class Array;
}  // namespace tint::ast

namespace tint::sem {

/// Array holds the semantic information for Array nodes.
class Array final : public Castable<Array, Type> {
  public:
    /// Constructor
    /// @param element the array element type
    /// @param count the number of elements in the array. 0 represents a
    /// runtime-sized array.
    /// @param align the byte alignment of the array
    /// @param size the byte size of the array
    /// @param stride the number of bytes from the start of one element of the
    /// array to the start of the next element
    /// @param implicit_stride the number of bytes from the start of one element
    /// of the array to the start of the next element, if there was no `@stride`
    /// attribute applied.
    Array(Type const* element,
          uint32_t count,
          uint32_t align,
          uint32_t size,
          uint32_t stride,
          uint32_t implicit_stride);

    /// @returns a hash of the type.
    size_t Hash() const override;

    /// @param other the other type to compare against
    /// @returns true if the this type is equal to the given type
    bool Equals(const Type& other) const override;

    /// @return the array element type
    Type const* ElemType() const { return element_; }

    /// @returns the number of elements in the array. 0 represents a runtime-sized
    /// array.
    uint32_t Count() const { return count_; }

    /// @returns the byte alignment of the array
    /// @note this may differ from the alignment of a structure member of this
    /// array type, if the member is annotated with the `@align(n)` attribute.
    uint32_t Align() const override;

    /// @returns the byte size of the array
    /// @note this may differ from the size of a structure member of this array
    /// type, if the member is annotated with the `@size(n)` attribute.
    uint32_t Size() const override;

    /// @returns the number of bytes from the start of one element of the
    /// array to the start of the next element
    uint32_t Stride() const { return stride_; }

    /// @returns the number of bytes from the start of one element of the
    /// array to the start of the next element, if there was no `@stride`
    /// attribute applied
    uint32_t ImplicitStride() const { return implicit_stride_; }

    /// @returns true if the value returned by Stride() matches the element's
    /// natural stride
    bool IsStrideImplicit() const { return stride_ == implicit_stride_; }

    /// @returns true if this array is runtime sized
    bool IsRuntimeSized() const { return count_ == 0; }

    /// @returns true if constructible as per
    /// https://gpuweb.github.io/gpuweb/wgsl/#constructible-types
    bool IsConstructible() const override;

    /// @param symbols the program's symbol table
    /// @returns the name for this type that closely resembles how it would be
    /// declared in WGSL.
    std::string FriendlyName(const SymbolTable& symbols) const override;

  private:
    Type const* const element_;
    const uint32_t count_;
    const uint32_t align_;
    const uint32_t size_;
    const uint32_t stride_;
    const uint32_t implicit_stride_;
    const bool constructible_;
};

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_ARRAY_H_
