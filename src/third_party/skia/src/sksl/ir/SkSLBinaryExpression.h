/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_BINARYEXPRESSION
#define SKSL_BINARYEXPRESSION

#include "include/core/SkTypes.h"
#include "include/sksl/SkSLOperator.h"
#include "include/sksl/SkSLPosition.h"
#include "src/sksl/ir/SkSLExpression.h"

#include <memory>
#include <string>
#include <utility>

namespace SkSL {

class Context;
class Type;

/**
 * A binary operation.
 */
class BinaryExpression final : public Expression {
public:
    inline static constexpr Kind kExpressionKind = Kind::kBinary;

    BinaryExpression(Position pos, std::unique_ptr<Expression> left, Operator op,
                     std::unique_ptr<Expression> right, const Type* type)
        : INHERITED(pos, kExpressionKind, type)
        , fLeft(std::move(left))
        , fOperator(op)
        , fRight(std::move(right)) {
        // If we are assigning to a VariableReference, ensure that it is set to Write or ReadWrite.
        SkASSERT(!op.isAssignment() || CheckRef(*this->left()));
    }

    // Creates a potentially-simplified form of the expression. Determines the result type
    // programmatically. Typechecks and coerces input expressions; reports errors via ErrorReporter.
    static std::unique_ptr<Expression> Convert(const Context& context,
                                               Position pos,
                                               std::unique_ptr<Expression> left,
                                               Operator op,
                                               std::unique_ptr<Expression> right);

    // Creates a potentially-simplified form of the expression. Determines the result type
    // programmatically. Asserts if the expressions do not typecheck or are otherwise invalid.
    static std::unique_ptr<Expression> Make(const Context& context,
                                            Position pos,
                                            std::unique_ptr<Expression> left,
                                            Operator op,
                                            std::unique_ptr<Expression> right);

    // Creates a potentially-simplified form of the expression. Result type is passed in.
    // Asserts if the expressions do not typecheck or are otherwise invalid.
    static std::unique_ptr<Expression> Make(const Context& context,
                                            Position pos,
                                            std::unique_ptr<Expression> left,
                                            Operator op,
                                            std::unique_ptr<Expression> right,
                                            const Type* resultType);

    std::unique_ptr<Expression>& left() {
        return fLeft;
    }

    const std::unique_ptr<Expression>& left() const {
        return fLeft;
    }

    std::unique_ptr<Expression>& right() {
        return fRight;
    }

    const std::unique_ptr<Expression>& right() const {
        return fRight;
    }

    Operator getOperator() const {
        return fOperator;
    }

    bool hasProperty(Property property) const override {
        if (property == Property::kSideEffects && this->getOperator().isAssignment()) {
            return true;
        }
        return this->left()->hasProperty(property) || this->right()->hasProperty(property);
    }

    std::unique_ptr<Expression> clone(Position pos) const override;

    std::string description() const override;

private:
    static bool CheckRef(const Expression& expr);

    std::unique_ptr<Expression> fLeft;
    Operator fOperator;
    std::unique_ptr<Expression> fRight;

    using INHERITED = Expression;
};

}  // namespace SkSL

#endif
