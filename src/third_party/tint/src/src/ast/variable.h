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

#ifndef SRC_AST_VARIABLE_H_
#define SRC_AST_VARIABLE_H_

#include <utility>
#include <vector>

#include "src/ast/decoration.h"
#include "src/ast/expression.h"
#include "src/ast/storage_class.h"

namespace tint {
namespace ast {

class BindingDecoration;
class GroupDecoration;
class LocationDecoration;

/// A Variable statement.
///
/// An instance of this class represents one of three constructs in WGSL: "var"
/// declaration, "const" declaration, or formal parameter to a function.
///
/// 1. A "var" declaration is a name for typed storage.  Examples:
///
///       // Declared outside a function, i.e. at module scope, requires
///       // a storage class.
///       var<workgroup> width : i32;     // no initializer
///       var<private> height : i32 = 3;  // with initializer
///
///       // A variable declared inside a function doesn't take a storage class,
///       // and maps to SPIR-V Function storage.
///       var computed_depth : i32;
///       var area : i32 = compute_area(width, height);
///
/// 2. A "const" declaration is a name for a typed value.  Examples:
///
///       const twice_depth : i32 = width + width;  // Must have initializer
///
/// 3. A formal parameter to a function is a name for a typed value to
///    be passed into a function.  Example:
///
///       fn twice(a: i32) -> i32 {  // "a:i32" is the formal parameter
///         return a + a;
///       }
///
/// From the WGSL draft, about "var"::
///
///   A variable is a named reference to storage that can contain a value of a
///   particular type.
///
///   Two types are associated with a variable: its store type (the type of
///   value that may be placed in the referenced storage) and its reference
///   type (the type of the variable itself).  If a variable has store type T
///   and storage class S, then its reference type is pointer-to-T-in-S.
///
/// This class uses the term "type" to refer to:
///     the value type of a "const",
///     the value type of the formal parameter,
///     or the store type of the "var".
///
/// The storage class for a "var" is StorageClass::kNone when using the
/// defaulting syntax for a "var" declared inside a function.
/// The storage class for a "const" is always StorageClass::kNone.
/// The storage class for a formal parameter is always StorageClass::kNone.
class Variable : public Castable<Variable, Node> {
 public:
  /// BindingPoint holds a group and binding decoration.
  struct BindingPoint {
    /// The `[[group]]` part of the binding point
    GroupDecoration* group = nullptr;
    /// The `[[binding]]` part of the binding point
    BindingDecoration* binding = nullptr;

    /// @returns true if the BindingPoint has a valid group and binding
    /// decoration.
    inline operator bool() const { return group && binding; }
  };

  /// Create a variable
  /// @param source the variable source
  /// @param sym the variable symbol
  /// @param declared_storage_class the declared storage class
  /// @param declared_type the declared variable type
  /// @param is_const true if the variable is const
  /// @param constructor the constructor expression
  /// @param decorations the variable decorations
  Variable(const Source& source,
           const Symbol& sym,
           StorageClass declared_storage_class,
           type::Type* declared_type,
           bool is_const,
           Expression* constructor,
           DecorationList decorations);
  /// Move constructor
  Variable(Variable&&);

  ~Variable() override;

  /// @returns the variable symbol
  const Symbol& symbol() const { return symbol_; }

  /// @returns the declared type
  type::Type* declared_type() const { return declared_type_; }

  /// @returns the declared storage class
  StorageClass declared_storage_class() const {
    return declared_storage_class_;
  }
  /// @returns the constructor expression or nullptr if none set
  Expression* constructor() const { return constructor_; }
  /// @returns true if the variable has an constructor
  bool has_constructor() const { return constructor_ != nullptr; }

  /// @returns true if this is a constant, false otherwise
  bool is_const() const { return is_const_; }

  /// @returns the decorations attached to this variable
  const DecorationList& decorations() const { return decorations_; }

  /// @returns the binding point information for the variable
  BindingPoint binding_point() const;

  /// @returns true if the decorations include a LocationDecoration
  bool HasLocationDecoration() const;
  /// @returns true if the decorations include a BuiltinDecoration
  bool HasBuiltinDecoration() const;
  /// @returns true if the decorations include a ConstantIdDecoration
  bool HasConstantIdDecoration() const;

  /// @returns pointer to LocationDecoration in decorations, otherwise NULL.
  LocationDecoration* GetLocationDecoration() const;

  /// @returns the constant_id value for the variable. Assumes that
  /// HasConstantIdDecoration() has been called first.
  uint32_t constant_id() const;

  /// Clones this node and all transitive child nodes using the `CloneContext`
  /// `ctx`.
  /// @param ctx the clone context
  /// @return the newly cloned node
  Variable* Clone(CloneContext* ctx) const override;

  /// Writes a representation of the node to the output stream
  /// @param sem the semantic info for the program
  /// @param out the stream to write to
  /// @param indent number of spaces to indent the node when writing
  void to_str(const semantic::Info& sem,
              std::ostream& out,
              size_t indent) const override;

 protected:
  /// Output information for this variable.
  /// @param sem the semantic info for the program
  /// @param out the stream to write to
  /// @param indent number of spaces to indent the node when writing
  void info_to_str(const semantic::Info& sem,
                   std::ostream& out,
                   size_t indent) const;
  /// Output constructor for this variable.
  /// @param sem the semantic info for the program
  /// @param out the stream to write to
  /// @param indent number of spaces to indent the node when writing
  void constructor_to_str(const semantic::Info& sem,
                          std::ostream& out,
                          size_t indent) const;

 private:
  Variable(const Variable&) = delete;

  Symbol const symbol_;
  // The value type if a const or formal paramter, and the store type if a var
  type::Type* const declared_type_;
  bool const is_const_;
  Expression* const constructor_;
  DecorationList const decorations_;
  StorageClass const declared_storage_class_;
};

/// A list of variables
using VariableList = std::vector<Variable*>;

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_VARIABLE_H_
