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

#ifndef SRC_SEMANTIC_TYPE_MAPPINGS_H_
#define SRC_SEMANTIC_TYPE_MAPPINGS_H_

#include <type_traits>

namespace tint {

// Forward declarations
namespace ast {
class CallExpression;
class Expression;
class Function;
class MemberAccessorExpression;
class Statement;
class StructMember;
class Variable;
}  // namespace ast
namespace type {
class Array;
class Struct;
}  // namespace type

namespace semantic {

// Forward declarations
class Array;
class Call;
class Expression;
class Function;
class MemberAccessorExpression;
class Statement;
class Struct;
class StructMember;
class Variable;

/// TypeMappings is a struct that holds dummy `operator()` methods that's used
/// by SemanticNodeTypeFor to map AST / type node types to their corresponding
/// semantic node types. The standard operator overload resolving rules will be
/// used to infer the return type based on the argument type.
struct TypeMappings {
  //! @cond Doxygen_Suppress
  Array* operator()(type::Array*);
  Call* operator()(ast::CallExpression*);
  Expression* operator()(ast::Expression*);
  Function* operator()(ast::Function*);
  MemberAccessorExpression* operator()(ast::MemberAccessorExpression*);
  Statement* operator()(ast::Statement*);
  Struct* operator()(type::Struct*);
  StructMember* operator()(ast::StructMember*);
  Variable* operator()(ast::Variable*);
  //! @endcond
};

/// SemanticNodeTypeFor resolves to the appropriate semantic::Node type for the
/// AST or type node `AST_OR_TYPE`.
template <typename AST_OR_TYPE>
using SemanticNodeTypeFor = typename std::remove_pointer<decltype(
    TypeMappings()(std::declval<AST_OR_TYPE*>()))>::type;

}  // namespace semantic
}  // namespace tint

#endif  // SRC_SEMANTIC_TYPE_MAPPINGS_H_
