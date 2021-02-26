/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_EXPRESSION
#define SKSL_EXPRESSION

#include "include/private/SkTHash.h"
#include "src/sksl/ir/SkSLStatement.h"
#include "src/sksl/ir/SkSLType.h"

#include <unordered_map>

namespace SkSL {

class Expression;
class IRGenerator;
class Variable;

using DefinitionMap = SkTHashMap<const Variable*, std::unique_ptr<Expression>*>;

/**
 * Abstract supertype of all expressions.
 */
class Expression : public IRNode {
public:
    enum class Kind {
        kBinary = (int) Statement::Kind::kLast + 1,
        kBoolLiteral,
        kConstructor,
        kDefined,
        kExternalFunctionCall,
        kExternalValue,
        kIntLiteral,
        kFieldAccess,
        kFloatLiteral,
        kFunctionReference,
        kFunctionCall,
        kIndex,
        kNullLiteral,
        kPrefix,
        kPostfix,
        kSetting,
        kSwizzle,
        kTernary,
        kTypeReference,
        kVariableReference,

        kFirst = kBinary,
        kLast = kVariableReference
    };

    enum class Property {
        kSideEffects,
        kContainsRTAdjust
    };

    Expression(int offset, Kind kind, const Type* type)
        : INHERITED(offset, (int) kind)
        , fType(type) {
        SkASSERT(kind >= Kind::kFirst && kind <= Kind::kLast);
    }

    Kind kind() const {
        return (Kind) fKind;
    }

    virtual const Type& type() const {
        return *fType;
    }

    /**
     *  Use is<T> to check the type of an expression.
     *  e.g. replace `e.kind() == Expression::Kind::kIntLiteral` with `e.is<IntLiteral>()`.
     */
    template <typename T>
    bool is() const {
        return this->kind() == T::kExpressionKind;
    }

    /**
     *  Use as<T> to downcast expressions: e.g. replace `(IntLiteral&) i` with `i.as<IntLiteral>()`.
     */
    template <typename T>
    const T& as() const {
        SkASSERT(this->is<T>());
        return static_cast<const T&>(*this);
    }

    template <typename T>
    T& as() {
        SkASSERT(this->is<T>());
        return static_cast<T&>(*this);
    }

    /**
     * Returns true if this expression is constant. compareConstant must be implemented for all
     * constants!
     */
    virtual bool isCompileTimeConstant() const {
        return false;
    }

    /**
     * Compares this constant expression against another constant expression of the same type. It is
     * an error to call this on non-constant expressions, or if the types of the expressions do not
     * match.
     */
    virtual bool compareConstant(const Context& context, const Expression& other) const {
        ABORT("cannot call compareConstant on this type");
    }

    /**
     * For an expression which evaluates to a constant int, returns the value. Otherwise calls
     * ABORT.
     */
    virtual int64_t getConstantInt() const {
        ABORT("not a constant int");
    }

    /**
     * For an expression which evaluates to a constant float, returns the value. Otherwise calls
     * ABORT.
     */
    virtual SKSL_FLOAT getConstantFloat() const {
        ABORT("not a constant float");
    }

    /**
     * Returns true if, given fixed values for uniforms, this expression always evaluates to the
     * same result with no side effects.
     */
    virtual bool isConstantOrUniform() const {
        SkASSERT(!this->isCompileTimeConstant() || !this->hasSideEffects());
        return this->isCompileTimeConstant();
    }

    virtual bool hasProperty(Property property) const = 0;

    bool hasSideEffects() const {
        return this->hasProperty(Property::kSideEffects);
    }

    bool containsRTAdjust() const {
        return this->hasProperty(Property::kContainsRTAdjust);
    }

    /**
     * Given a map of known constant variable values, substitute them in for references to those
     * variables occurring in this expression and its subexpressions.  Similar simplifications, such
     * as folding a constant binary expression down to a single value, may also be performed.
     * Returns a new expression which replaces this expression, or null if no replacements were
     * made. If a new expression is returned, this expression is no longer valid.
     */
    virtual std::unique_ptr<Expression> constantPropagate(const IRGenerator& irGenerator,
                                                          const DefinitionMap& definitions) {
        return nullptr;
    }

    virtual CoercionCost coercionCost(const Type& target) const {
        return this->type().coercionCost(target);
    }

    /**
     * For a vector of floating point values, return the value of the n'th vector component. It is
     * an error to call this method on an expression which is not a vector of FloatLiterals.
     */
    virtual SKSL_FLOAT getFVecComponent(int n) const {
        SkASSERT(false);
        return 0;
    }

    /**
     * For a vector of integer values, return the value of the n'th vector component. It is an error
     * to call this method on an expression which is not a vector of IntLiterals.
     */
    virtual SKSL_INT getIVecComponent(int n) const {
        SkASSERT(false);
        return 0;
    }

    /**
     * For a vector of literals, return the value of the n'th vector component. It is an error to
     * call this method on an expression which is not a vector of Literal<T>.
     */
    template <typename T> T getVecComponent(int index) const;

    /**
     * For a literal matrix expression, return the floating point value of the component at
     * [col][row]. It is an error to call this method on an expression which is not a literal
     * matrix.
     */
    virtual SKSL_FLOAT getMatComponent(int col, int row) const {
        SkASSERT(false);
        return 0;
    }

    virtual std::unique_ptr<Expression> clone() const = 0;

private:
    const Type* fType;

    using INHERITED = IRNode;
};

template <> inline SKSL_FLOAT Expression::getVecComponent<SKSL_FLOAT>(int index) const {
    return this->getFVecComponent(index);
}

template <> inline SKSL_INT Expression::getVecComponent<SKSL_INT>(int index) const {
    return this->getIVecComponent(index);
}

}  // namespace SkSL

#endif
