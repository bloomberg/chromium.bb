/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_CONSTRUCTOR_SPLAT
#define SKSL_CONSTRUCTOR_SPLAT

#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLConstructor.h"
#include "src/sksl/ir/SkSLExpression.h"

#include <memory>

namespace SkSL {

/**
 * Represents the construction of a vector splat, such as `half3(n)`.
 *
 * These always contain exactly 1 scalar.
 */
class ConstructorSplat final : public SingleArgumentConstructor {
public:
    static constexpr Kind kExpressionKind = Kind::kConstructorSplat;

    ConstructorSplat(int offset, const Type& type, std::unique_ptr<Expression> arg)
        : INHERITED(offset, kExpressionKind, &type, std::move(arg)) {}

    // The input argument must be scalar. A "splat" to a scalar type will be optimized into a no-op.
    static std::unique_ptr<Expression> Make(const Context& context,
                                            int offset,
                                            const Type& type,
                                            std::unique_ptr<Expression> arg);

    std::unique_ptr<Expression> clone() const override {
        return std::make_unique<ConstructorSplat>(fOffset, this->type(), argument()->clone());
    }

    bool allowsConstantSubexpressions() const override {
        return true;
    }

    const Expression* getConstantSubexpression(int n) const override {
        SkASSERT(n >= 0 && n < this->type().columns());
        return this->argument()->getConstantSubexpression(0);
    }

private:
    using INHERITED = SingleArgumentConstructor;
};

}  // namespace SkSL

#endif
