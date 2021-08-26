/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLConstructorArrayCast.h"

#include "src/sksl/SkSLConstantFolder.h"
#include "src/sksl/ir/SkSLConstructorArray.h"
#include "src/sksl/ir/SkSLConstructorCompoundCast.h"
#include "src/sksl/ir/SkSLConstructorScalarCast.h"

namespace SkSL {

static std::unique_ptr<Expression> cast_constant_array(const Context& context,
                                                       const Type& destType,
                                                       std::unique_ptr<Expression> constCtor) {
    const Type& scalarType = destType.componentType();

    // Create a ConstructorArray(...) which typecasts each argument inside.
    auto inputArgs = constCtor->as<ConstructorArray>().argumentSpan();
    ExpressionArray typecastArgs;
    typecastArgs.reserve_back(inputArgs.size());
    for (std::unique_ptr<Expression>& arg : inputArgs) {
        int offset = arg->fOffset;
        if (arg->type().isScalar()) {
            typecastArgs.push_back(ConstructorScalarCast::Make(context, offset, scalarType,
                                                               std::move(arg)));
        } else {
            typecastArgs.push_back(ConstructorCompoundCast::Make(context, offset, scalarType,
                                                                 std::move(arg)));
        }
    }

    return ConstructorArray::Make(context, constCtor->fOffset, destType, std::move(typecastArgs));
}

std::unique_ptr<Expression> ConstructorArrayCast::Make(const Context& context,
                                                       int offset,
                                                       const Type& type,
                                                       std::unique_ptr<Expression> arg) {
    // Only arrays of the same size are allowed.
    SkASSERT(type.isArray());
    SkASSERT(arg->type().isArray());
    SkASSERT(type.columns() == arg->type().columns());

    // If this is a no-op cast, return the expression as-is.
    if (type == arg->type()) {
        return arg;
    }

    // When optimization is on, look up the value of constant variables. This allows expressions
    // like `myArray` to be replaced with the compile-time constant `int[2](0, 1)`.
    if (context.fConfig->fSettings.fOptimize) {
        arg = ConstantFolder::MakeConstantValueForVariable(std::move(arg));
    }
    // We can cast a vector of compile-time constants at compile-time.
    if (arg->isCompileTimeConstant()) {
        return cast_constant_array(context, type, std::move(arg));
    }
    return std::make_unique<ConstructorArrayCast>(offset, type, std::move(arg));
}

}  // namespace SkSL
