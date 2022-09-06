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

#ifndef SRC_TINT_RESOLVER_INTRINSIC_TABLE_H_
#define SRC_TINT_RESOLVER_INTRINSIC_TABLE_H_

#include <memory>
#include <string>
#include <vector>

#include "src/tint/resolver/const_eval.h"
#include "src/tint/resolver/ctor_conv_intrinsic.h"
#include "src/tint/sem/builtin.h"

// Forward declarations
namespace tint {
class ProgramBuilder;
}  // namespace tint

namespace tint::resolver {

/// IntrinsicTable is a lookup table of all the WGSL builtin functions and intrinsic operators
class IntrinsicTable {
  public:
    /// @param builder the program builder
    /// @return a pointer to a newly created IntrinsicTable
    static std::unique_ptr<IntrinsicTable> Create(ProgramBuilder& builder);

    /// Destructor
    virtual ~IntrinsicTable();

    /// Builtin describes a resolved builtin function
    struct Builtin {
        /// The semantic info for the builtin
        const sem::Builtin* sem = nullptr;
        /// The constant evaluation function
        const_eval::Function* const_eval_fn = nullptr;
    };

    /// UnaryOperator describes a resolved unary operator
    struct UnaryOperator {
        /// The result type of the unary operator
        const sem::Type* result = nullptr;
        /// The type of the parameter of the unary operator
        const sem::Type* parameter = nullptr;
    };

    /// BinaryOperator describes a resolved binary operator
    struct BinaryOperator {
        /// The result type of the binary operator
        const sem::Type* result = nullptr;
        /// The type of LHS parameter of the binary operator
        const sem::Type* lhs = nullptr;
        /// The type of RHS parameter of the binary operator
        const sem::Type* rhs = nullptr;
    };

    /// Lookup looks for the builtin overload with the given signature, raising an error diagnostic
    /// if the builtin was not found.
    /// @param type the builtin type
    /// @param args the argument types passed to the builtin function
    /// @param source the source of the builtin call
    /// @return the semantic builtin if found, otherwise nullptr
    virtual Builtin Lookup(sem::BuiltinType type,
                           const std::vector<const sem::Type*>& args,
                           const Source& source) = 0;

    /// Lookup looks for the unary op overload with the given signature, raising an error
    /// diagnostic if the operator was not found.
    /// @param op the unary operator
    /// @param arg the type of the expression passed to the operator
    /// @param source the source of the operator call
    /// @return the operator call target signature. If the operator was not found
    ///         UnaryOperator::result will be nullptr.
    virtual UnaryOperator Lookup(ast::UnaryOp op, const sem::Type* arg, const Source& source) = 0;

    /// Lookup looks for the binary op overload with the given signature, raising an error
    /// diagnostic if the operator was not found.
    /// @param op the binary operator
    /// @param lhs the LHS value type passed to the operator
    /// @param rhs the RHS value type passed to the operator
    /// @param source the source of the operator call
    /// @param is_compound true if the binary operator is being used as a compound assignment
    /// @return the operator call target signature. If the operator was not found
    ///         BinaryOperator::result will be nullptr.
    virtual BinaryOperator Lookup(ast::BinaryOp op,
                                  const sem::Type* lhs,
                                  const sem::Type* rhs,
                                  const Source& source,
                                  bool is_compound) = 0;

    /// Lookup looks for the type constructor or conversion overload for the given
    /// CtorConvIntrinsic.
    /// @param type the type being constructed or converted
    /// @param template_arg the optional template argument
    /// @param args the argument types passed to the constructor / conversion call
    /// @param source the source of the call
    /// @return a sem::TypeConstructor, sem::TypeConversion or nullptr if nothing matched
    virtual const sem::CallTarget* Lookup(CtorConvIntrinsic type,
                                          const sem::Type* template_arg,
                                          const std::vector<const sem::Type*>& args,
                                          const Source& source) = 0;
};

}  // namespace tint::resolver

#endif  // SRC_TINT_RESOLVER_INTRINSIC_TABLE_H_
