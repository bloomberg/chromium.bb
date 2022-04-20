/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLFieldAccess.h"
#include "src/sksl/ir/SkSLMethodReference.h"
#include "src/sksl/ir/SkSLSetting.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLUnresolvedFunction.h"

namespace SkSL {

std::unique_ptr<Expression> FieldAccess::Convert(const Context& context,
                                                 Position pos,
                                                 SymbolTable& symbolTable,
                                                 std::unique_ptr<Expression> base,
                                                 std::string_view field) {
    const Type& baseType = base->type();
    if (baseType.isEffectChild()) {
        // Turn the field name into a free function name, prefixed with '$':
        std::string methodName = "$" + std::string(field);
        const Symbol* result = symbolTable[methodName];
        if (result) {
            switch (result->kind()) {
                case Symbol::Kind::kFunctionDeclaration: {
                    std::vector<const FunctionDeclaration*> f = {
                            &result->as<FunctionDeclaration>()};
                    return std::make_unique<MethodReference>(
                            context, pos, std::move(base), f);
                }
                case Symbol::Kind::kUnresolvedFunction: {
                    const UnresolvedFunction& f = result->as<UnresolvedFunction>();
                    return std::make_unique<MethodReference>(
                            context, pos, std::move(base), f.functions());
                }
                default:
                    break;
            }
        }
        context.fErrors->error(pos, "type '" + baseType.displayName() + "' has no method named '" +
                std::string(field) + "'");
        return nullptr;
    }
    if (baseType.isStruct()) {
        const std::vector<Type::Field>& fields = baseType.fields();
        for (size_t i = 0; i < fields.size(); i++) {
            if (fields[i].fName == field) {
                return FieldAccess::Make(context, pos, std::move(base), (int) i);
            }
        }
    }
    if (baseType.matches(*context.fTypes.fSkCaps)) {
        return Setting::Convert(context, pos, field);
    }

    context.fErrors->error(pos, "type '" + baseType.displayName() +
            "' does not have a field named '" + std::string(field) + "'");
    return nullptr;
}

std::unique_ptr<Expression> FieldAccess::Make(const Context& context,
                                              Position pos,
                                              std::unique_ptr<Expression> base,
                                              int fieldIndex,
                                              OwnerKind ownerKind) {
    SkASSERT(base->type().isStruct());
    SkASSERT(fieldIndex >= 0);
    SkASSERT(fieldIndex < (int) base->type().fields().size());
    return std::make_unique<FieldAccess>(pos, std::move(base), fieldIndex, ownerKind);
}

}  // namespace SkSL
