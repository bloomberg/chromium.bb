/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLConstantFolder.h"

#include <limits>

#include "include/sksl/SkSLErrorReporter.h"
#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLBinaryExpression.h"
#include "src/sksl/ir/SkSLBoolLiteral.h"
#include "src/sksl/ir/SkSLConstructor.h"
#include "src/sksl/ir/SkSLConstructorCompound.h"
#include "src/sksl/ir/SkSLConstructorSplat.h"
#include "src/sksl/ir/SkSLExpression.h"
#include "src/sksl/ir/SkSLFloatLiteral.h"
#include "src/sksl/ir/SkSLIntLiteral.h"
#include "src/sksl/ir/SkSLPrefixExpression.h"
#include "src/sksl/ir/SkSLType.h"
#include "src/sksl/ir/SkSLVariable.h"
#include "src/sksl/ir/SkSLVariableReference.h"

namespace SkSL {

static std::unique_ptr<Expression> eliminate_no_op_boolean(const Expression& left,
                                                           Operator op,
                                                           const Expression& right) {
    SkASSERT(right.is<BoolLiteral>());
    bool rightVal = right.as<BoolLiteral>().value();

    // Detect no-op Boolean expressions and optimize them away.
    if ((op.kind() == Token::Kind::TK_LOGICALAND && rightVal)  ||  // (expr && true)  -> (expr)
        (op.kind() == Token::Kind::TK_LOGICALOR  && !rightVal) ||  // (expr || false) -> (expr)
        (op.kind() == Token::Kind::TK_LOGICALXOR && !rightVal) ||  // (expr ^^ false) -> (expr)
        (op.kind() == Token::Kind::TK_EQEQ       && rightVal)  ||  // (expr == true)  -> (expr)
        (op.kind() == Token::Kind::TK_NEQ        && !rightVal)) {  // (expr != false) -> (expr)

        return left.clone();
    }

    return nullptr;
}

static std::unique_ptr<Expression> short_circuit_boolean(const Expression& left,
                                                         Operator op,
                                                         const Expression& right) {
    SkASSERT(left.is<BoolLiteral>());
    bool leftVal = left.as<BoolLiteral>().value();

    // When the literal is on the left, we can sometimes eliminate the other expression entirely.
    if ((op.kind() == Token::Kind::TK_LOGICALAND && !leftVal) ||  // (false && expr) -> (false)
        (op.kind() == Token::Kind::TK_LOGICALOR  && leftVal)) {   // (true  || expr) -> (true)

        return left.clone();
    }

    // We can't eliminate the right-side expression via short-circuit, but we might still be able to
    // simplify away a no-op expression.
    return eliminate_no_op_boolean(right, op, left);
}

static std::unique_ptr<Expression> simplify_vector_equality(const Context& context,
                                                            const Expression& left,
                                                            Operator op,
                                                            const Expression& right) {
    if (op.kind() == Token::Kind::TK_EQEQ || op.kind() == Token::Kind::TK_NEQ) {
        bool equality = (op.kind() == Token::Kind::TK_EQEQ);

        switch (left.compareConstant(right)) {
            case Expression::ComparisonResult::kNotEqual:
                equality = !equality;
                [[fallthrough]];

            case Expression::ComparisonResult::kEqual:
                return BoolLiteral::Make(context, left.fOffset, equality);

            case Expression::ComparisonResult::kUnknown:
                break;
        }
    }
    return nullptr;
}

// 'T' is the actual stored type of the literal data (SKSL_FLOAT or SKSL_INT).
// 'U' is an unsigned version of that, used to perform addition, subtraction, and multiplication,
// to avoid signed-integer overflow errors. This mimics the use of URESULT vs. RESULT when doing
// scalar folding in Simplify, later in this file.
template <typename T, typename U = T>
static std::unique_ptr<Expression> simplify_vector(const Context& context,
                                                   const Expression& left,
                                                   Operator op,
                                                   const Expression& right) {
    SkASSERT(left.type().isVector());
    SkASSERT(left.type() == right.type());
    const Type& type = left.type();

    // Handle equality operations: == !=
    if (std::unique_ptr<Expression> result = simplify_vector_equality(context, left, op, right)) {
        return result;
    }

    // Handle floating-point arithmetic: + - * /
    const auto vectorComponentwiseFold = [&](auto foldFn) -> std::unique_ptr<Expression> {
        const Type& componentType = type.componentType();
        ExpressionArray args;
        args.reserve_back(type.columns());
        for (int i = 0; i < type.columns(); i++) {
            U value = foldFn(left.getConstantSubexpression(i)->as<Literal<T>>().value(),
                             right.getConstantSubexpression(i)->as<Literal<T>>().value());
            args.push_back(Literal<T>::Make(left.fOffset, value, &componentType));
        }
        return ConstructorCompound::Make(context, left.fOffset, type, std::move(args));
    };

    switch (op.kind()) {
        case Token::Kind::TK_PLUS:  return vectorComponentwiseFold([](U a, U b) { return a + b; });
        case Token::Kind::TK_MINUS: return vectorComponentwiseFold([](U a, U b) { return a - b; });
        case Token::Kind::TK_STAR:  return vectorComponentwiseFold([](U a, U b) { return a * b; });
        case Token::Kind::TK_SLASH: return vectorComponentwiseFold([](T a, T b) { return a / b; });
        default:
            return nullptr;
    }
}

static std::unique_ptr<Expression> cast_expression(const Context& context,
                                                   const Expression& expr,
                                                   const Type& type) {
    ExpressionArray ctorArgs;
    ctorArgs.push_back(expr.clone());
    std::unique_ptr<Expression> ctor = Constructor::Convert(context, expr.fOffset, type,
                                                            std::move(ctorArgs));
    SkASSERT(ctor);
    return ctor;
}

static ConstructorSplat splat_scalar(const Expression& scalar, const Type& type) {
    SkASSERT(type.isVector());
    SkASSERT(type.componentType() == scalar.type());

    // Use a constructor to splat the scalar expression across a vector.
    return ConstructorSplat{scalar.fOffset, type, scalar.clone()};
}

bool ConstantFolder::GetConstantInt(const Expression& value, SKSL_INT* out) {
    const Expression* expr = GetConstantValueForVariable(value);
    if (!expr->is<IntLiteral>()) {
        return false;
    }
    *out = expr->as<IntLiteral>().value();
    return true;
}

bool ConstantFolder::GetConstantFloat(const Expression& value, SKSL_FLOAT* out) {
    const Expression* expr = GetConstantValueForVariable(value);
    if (!expr->is<FloatLiteral>()) {
        return false;
    }
    *out = expr->as<FloatLiteral>().value();
    return true;
}

static bool is_constant_scalar_value(const Expression& inExpr, float match) {
    const Expression* expr = ConstantFolder::GetConstantValueForVariable(inExpr);
    return (expr->is<IntLiteral>()   && expr->as<IntLiteral>().value()   == match) ||
           (expr->is<FloatLiteral>() && expr->as<FloatLiteral>().value() == match);
}

static bool contains_constant_zero(const Expression& expr) {
    int numSlots = expr.type().slotCount();
    for (int index = 0; index < numSlots; ++index) {
        const Expression* subexpr = expr.getConstantSubexpression(index);
        if (subexpr && is_constant_scalar_value(*subexpr, 0.0f)) {
            return true;
        }
    }
    return false;
}

static bool is_constant_value(const Expression& expr, float value) {
    int numSlots = expr.type().slotCount();
    for (int index = 0; index < numSlots; ++index) {
        const Expression* subexpr = expr.getConstantSubexpression(index);
        if (!subexpr || !is_constant_scalar_value(*subexpr, value)) {
            return false;
        }
    }
    return true;
}

bool ConstantFolder::ErrorOnDivideByZero(const Context& context, int offset, Operator op,
                                         const Expression& right) {
    switch (op.kind()) {
        case Token::Kind::TK_SLASH:
        case Token::Kind::TK_SLASHEQ:
        case Token::Kind::TK_PERCENT:
        case Token::Kind::TK_PERCENTEQ:
            if (contains_constant_zero(right)) {
                context.fErrors->error(offset, "division by zero");
                return true;
            }
            return false;
        default:
            return false;
    }
}

const Expression* ConstantFolder::GetConstantValueForVariable(const Expression& inExpr) {
    for (const Expression* expr = &inExpr;;) {
        if (!expr->is<VariableReference>()) {
            break;
        }
        const VariableReference& varRef = expr->as<VariableReference>();
        if (varRef.refKind() != VariableRefKind::kRead) {
            break;
        }
        const Variable& var = *varRef.variable();
        if (!(var.modifiers().fFlags & Modifiers::kConst_Flag)) {
            break;
        }
        expr = var.initialValue();
        if (!expr) {
            // Function parameters can be const but won't have an initial value.
            break;
        }
        if (expr->isCompileTimeConstant()) {
            return expr;
        }
    }
    // We didn't find a compile-time constant at the end. Return the expression as-is.
    return &inExpr;
}

std::unique_ptr<Expression> ConstantFolder::MakeConstantValueForVariable(
        std::unique_ptr<Expression> expr) {
    const Expression* constantExpr = GetConstantValueForVariable(*expr);
    if (constantExpr != expr.get()) {
        expr = constantExpr->clone();
    }
    return expr;
}

static std::unique_ptr<Expression> simplify_no_op_arithmetic(const Context& context,
                                                             const Expression& left,
                                                             Operator op,
                                                             const Expression& right,
                                                             const Type& resultType) {
    switch (op.kind()) {
        case Token::Kind::TK_PLUS:
            if (is_constant_value(right, 0.0)) {  // x + 0
                return cast_expression(context, left, resultType);
            }
            if (is_constant_value(left, 0.0)) {   // 0 + x
                return cast_expression(context, right, resultType);
            }
            break;

        case Token::Kind::TK_STAR:
            if (is_constant_value(right, 1.0)) {  // x * 1
                return cast_expression(context, left, resultType);
            }
            if (is_constant_value(left, 1.0)) {   // 1 * x
                return cast_expression(context, right, resultType);
            }
            if (is_constant_value(right, 0.0) && !left.hasSideEffects()) {  // x * 0
                return cast_expression(context, right, resultType);
            }
            if (is_constant_value(left, 0.0) && !right.hasSideEffects()) {  // 0 * x
                return cast_expression(context, left, resultType);
            }
            break;

        case Token::Kind::TK_MINUS:
            if (is_constant_value(right, 0.0)) {  // x - 0
                return cast_expression(context, left, resultType);
            }
            if (is_constant_value(left, 0.0)) {   // 0 - x (to `-x`)
                return PrefixExpression::Make(context, Token::Kind::TK_MINUS,
                                              cast_expression(context, right, resultType));
            }
            break;

        case Token::Kind::TK_SLASH:
            if (is_constant_value(right, 1.0)) {  // x / 1
                return cast_expression(context, left, resultType);
            }
            break;

        case Token::Kind::TK_PLUSEQ:
        case Token::Kind::TK_MINUSEQ:
            if (is_constant_value(right, 0.0)) {  // x += 0, x -= 0
                std::unique_ptr<Expression> result = cast_expression(context, left, resultType);
                Analysis::UpdateVariableRefKind(result.get(), VariableRefKind::kRead);
                return result;
            }
            break;

        case Token::Kind::TK_STAREQ:
        case Token::Kind::TK_SLASHEQ:
            if (is_constant_value(right, 1.0)) {  // x *= 1, x /= 1
                std::unique_ptr<Expression> result = cast_expression(context, left, resultType);
                Analysis::UpdateVariableRefKind(result.get(), VariableRefKind::kRead);
                return result;
            }
            break;

        default:
            break;
    }

    return nullptr;
}

template <typename T>
static std::unique_ptr<Expression> fold_float_expression(int offset,
                                                         T result,
                                                         const Type* resultType) {
    // If constant-folding this expression would generate a NaN/infinite result, leave it as-is.
    if constexpr (!std::is_same<T, bool>::value) {
        if (!std::isfinite(result)) {
            return nullptr;
        }
    }

    return Literal<T>::Make(offset, result, resultType);
}

template <typename T>
static std::unique_ptr<Expression> fold_int_expression(int offset,
                                                       T result,
                                                       const Type* resultType) {
    // If constant-folding this expression would overflow the result type, leave it as-is.
    if constexpr (!std::is_same<T, bool>::value) {
        if (result < resultType->minimumValue() || result > resultType->maximumValue()) {
            return nullptr;
        }
    }

    return Literal<T>::Make(offset, result, resultType);
}

std::unique_ptr<Expression> ConstantFolder::Simplify(const Context& context,
                                                     int offset,
                                                     const Expression& leftExpr,
                                                     Operator op,
                                                     const Expression& rightExpr,
                                                     const Type& resultType) {
    // When optimization is enabled, replace constant variables with trivial initial-values.
    const Expression* left;
    const Expression* right;
    if (context.fConfig->fSettings.fOptimize) {
        left = GetConstantValueForVariable(leftExpr);
        right = GetConstantValueForVariable(rightExpr);
    } else {
        left = &leftExpr;
        right = &rightExpr;
    }

    // If this is the comma operator, the left side is evaluated but not otherwise used in any way.
    // So if the left side has no side effects, it can just be eliminated entirely.
    if (op.kind() == Token::Kind::TK_COMMA && !left->hasSideEffects()) {
        return right->clone();
    }

    // If this is the assignment operator, and both sides are the same trivial expression, this is
    // self-assignment (i.e., `var = var`) and can be reduced to just a variable reference (`var`).
    // This can happen when other parts of the assignment are optimized away.
    if (op.kind() == Token::Kind::TK_EQ && Analysis::IsSameExpressionTree(*left, *right)) {
        return right->clone();
    }

    // Simplify the expression when both sides are constant Boolean literals.
    if (left->is<BoolLiteral>() && right->is<BoolLiteral>()) {
        bool leftVal  = left->as<BoolLiteral>().value();
        bool rightVal = right->as<BoolLiteral>().value();
        bool result;
        switch (op.kind()) {
            case Token::Kind::TK_LOGICALAND: result = leftVal && rightVal; break;
            case Token::Kind::TK_LOGICALOR:  result = leftVal || rightVal; break;
            case Token::Kind::TK_LOGICALXOR: result = leftVal ^  rightVal; break;
            case Token::Kind::TK_EQEQ:       result = leftVal == rightVal; break;
            case Token::Kind::TK_NEQ:        result = leftVal != rightVal; break;
            default: return nullptr;
        }
        return BoolLiteral::Make(context, offset, result);
    }

    // If the left side is a Boolean literal, apply short-circuit optimizations.
    if (left->is<BoolLiteral>()) {
        return short_circuit_boolean(*left, op, *right);
    }

    // If the right side is a Boolean literal...
    if (right->is<BoolLiteral>()) {
        // ... and the left side has no side effects...
        if (!left->hasSideEffects()) {
            // We can reverse the expressions and short-circuit optimizations are still valid.
            return short_circuit_boolean(*right, op, *left);
        }

        // We can't use short-circuiting, but we can still optimize away no-op Boolean expressions.
        return eliminate_no_op_boolean(*left, op, *right);
    }

    if (op.kind() == Token::Kind::TK_EQEQ && Analysis::IsSameExpressionTree(*left, *right)) {
        // With == comparison, if both sides are the same trivial expression, this is self-
        // comparison and is always true. (We are not concerned with NaN.)
        return BoolLiteral::Make(context, leftExpr.fOffset, /*value=*/true);
    }

    if (op.kind() == Token::Kind::TK_NEQ && Analysis::IsSameExpressionTree(*left, *right)) {
        // With != comparison, if both sides are the same trivial expression, this is self-
        // comparison and is always false. (We are not concerned with NaN.)
        return BoolLiteral::Make(context, leftExpr.fOffset, /*value=*/false);
    }

    if (ErrorOnDivideByZero(context, offset, op, *right)) {
        return nullptr;
    }

    // Optimize away no-op arithmetic like `x * 1`, `x *= 1`, `x + 0`, `x * 0`, `0 / x`, etc.
    const Type& leftType = left->type();
    const Type& rightType = right->type();
    if ((leftType.isScalar() || leftType.isVector()) &&
        (rightType.isScalar() || rightType.isVector())) {
        std::unique_ptr<Expression> expr = simplify_no_op_arithmetic(context, *left, op, *right,
                                                                     resultType);
        if (expr) {
            return expr;
        }
    }

    // Other than the cases above, constant folding requires both sides to be constant.
    if (!left->isCompileTimeConstant() || !right->isCompileTimeConstant()) {
        return nullptr;
    }

    // Note that we expressly do not worry about precision and overflow here -- we use the maximum
    // precision to calculate the results and hope the result makes sense.
    // TODO(skia:10932): detect and handle integer overflow properly.
    using SKSL_UINT = uint64_t;
    if (left->is<IntLiteral>() && right->is<IntLiteral>()) {
        SKSL_INT leftVal  = left->as<IntLiteral>().value();
        SKSL_INT rightVal = right->as<IntLiteral>().value();

        #define RESULT(Op)   fold_int_expression(offset, \
                                        (SKSL_INT)(leftVal) Op (SKSL_INT)(rightVal), &resultType)
        #define URESULT(Op)  fold_int_expression(offset, \
                             (SKSL_INT)((SKSL_UINT)(leftVal) Op (SKSL_UINT)(rightVal)), &resultType)
        switch (op.kind()) {
            case Token::Kind::TK_PLUS:       return URESULT(+);
            case Token::Kind::TK_MINUS:      return URESULT(-);
            case Token::Kind::TK_STAR:       return URESULT(*);
            case Token::Kind::TK_SLASH:
                if (leftVal == std::numeric_limits<SKSL_INT>::min() && rightVal == -1) {
                    context.fErrors->error(offset, "arithmetic overflow");
                    return nullptr;
                }
                return RESULT(/);
            case Token::Kind::TK_PERCENT:
                if (leftVal == std::numeric_limits<SKSL_INT>::min() && rightVal == -1) {
                    context.fErrors->error(offset, "arithmetic overflow");
                    return nullptr;
                }
                return RESULT(%);
            case Token::Kind::TK_BITWISEAND: return RESULT(&);
            case Token::Kind::TK_BITWISEOR:  return RESULT(|);
            case Token::Kind::TK_BITWISEXOR: return RESULT(^);
            case Token::Kind::TK_EQEQ:       return RESULT(==);
            case Token::Kind::TK_NEQ:        return RESULT(!=);
            case Token::Kind::TK_GT:         return RESULT(>);
            case Token::Kind::TK_GTEQ:       return RESULT(>=);
            case Token::Kind::TK_LT:         return RESULT(<);
            case Token::Kind::TK_LTEQ:       return RESULT(<=);
            case Token::Kind::TK_SHL:
                if (rightVal >= 0 && rightVal <= 31) {
                    // Left-shifting a negative (or really, any signed) value is undefined behavior
                    // in C++, but not GLSL. Do the shift on unsigned values, to avoid UBSAN.
                    return URESULT(<<);
                }
                context.fErrors->error(offset, "shift value out of range");
                return nullptr;
            case Token::Kind::TK_SHR:
                if (rightVal >= 0 && rightVal <= 31) {
                    return RESULT(>>);
                }
                context.fErrors->error(offset, "shift value out of range");
                return nullptr;

            default:
                return nullptr;
        }
        #undef RESULT
        #undef URESULT
    }

    // Perform constant folding on pairs of floating-point literals.
    if (left->is<FloatLiteral>() && right->is<FloatLiteral>()) {
        SKSL_FLOAT leftVal  = left->as<FloatLiteral>().value();
        SKSL_FLOAT rightVal = right->as<FloatLiteral>().value();

        #define RESULT(Op) fold_float_expression(offset, leftVal Op rightVal, &resultType)
        switch (op.kind()) {
            case Token::Kind::TK_PLUS:  return RESULT(+);
            case Token::Kind::TK_MINUS: return RESULT(-);
            case Token::Kind::TK_STAR:  return RESULT(*);
            case Token::Kind::TK_SLASH: return RESULT(/);
            case Token::Kind::TK_EQEQ:  return RESULT(==);
            case Token::Kind::TK_NEQ:   return RESULT(!=);
            case Token::Kind::TK_GT:    return RESULT(>);
            case Token::Kind::TK_GTEQ:  return RESULT(>=);
            case Token::Kind::TK_LT:    return RESULT(<);
            case Token::Kind::TK_LTEQ:  return RESULT(<=);
            default:                    return nullptr;
        }
        #undef RESULT
    }

    // Perform constant folding on pairs of vectors.
    if (leftType.isVector() && leftType == rightType) {
        if (leftType.componentType().isFloat()) {
            return simplify_vector<SKSL_FLOAT>(context, *left, op, *right);
        }
        if (leftType.componentType().isInteger()) {
            return simplify_vector<SKSL_INT, SKSL_UINT>(context, *left, op, *right);
        }
        if (leftType.componentType().isBoolean()) {
            return simplify_vector_equality(context, *left, op, *right);
        }
        return nullptr;
    }

    // Perform constant folding on vectors against scalars, e.g.: half4(2) + 2
    if (leftType.isVector() && leftType.componentType() == rightType) {
        if (rightType.isFloat()) {
            return simplify_vector<SKSL_FLOAT>(context, *left, op,
                                               splat_scalar(*right, left->type()));
        }
        if (rightType.isInteger()) {
            return simplify_vector<SKSL_INT, SKSL_UINT>(context, *left, op,
                                                        splat_scalar(*right, left->type()));
        }
        if (rightType.isBoolean()) {
            return simplify_vector_equality(context, *left, op,
                                            splat_scalar(*right, left->type()));
        }
        return nullptr;
    }

    // Perform constant folding on scalars against vectors, e.g.: 2 + half4(2)
    if (rightType.isVector() && rightType.componentType() == leftType) {
        if (leftType.isFloat()) {
            return simplify_vector<SKSL_FLOAT>(context, splat_scalar(*left, right->type()), op,
                                               *right);
        }
        if (leftType.isInteger()) {
            return simplify_vector<SKSL_INT, SKSL_UINT>(context, splat_scalar(*left, right->type()),
                                                        op, *right);
        }
        if (leftType.isBoolean()) {
            return simplify_vector_equality(context, splat_scalar(*left, right->type()),
                                            op, *right);
        }
        return nullptr;
    }

    // Perform constant folding on pairs of matrices or arrays.
    if ((leftType.isMatrix() && rightType.isMatrix()) ||
        (leftType.isArray() && rightType.isArray())) {
        bool equality;
        switch (op.kind()) {
            case Token::Kind::TK_EQEQ:
                equality = true;
                break;
            case Token::Kind::TK_NEQ:
                equality = false;
                break;
            default:
                return nullptr;
        }

        switch (left->compareConstant(*right)) {
            case Expression::ComparisonResult::kNotEqual:
                equality = !equality;
                [[fallthrough]];

            case Expression::ComparisonResult::kEqual:
                return BoolLiteral::Make(context, offset, equality);

            case Expression::ComparisonResult::kUnknown:
                return nullptr;
        }
    }

    // We aren't able to constant-fold.
    return nullptr;
}

}  // namespace SkSL
