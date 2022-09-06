/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_VARIABLE
#define SKSL_VARIABLE

#include "include/core/SkTypes.h"
#include "include/private/SkSLModifiers.h"
#include "include/private/SkSLStatement.h"
#include "include/private/SkSLSymbol.h"
#include "include/sksl/SkSLPosition.h"
#include "src/sksl/ir/SkSLType.h"

#include <memory>
#include <string>
#include <string_view>

namespace SkSL {

class Context;
class Expression;
class SymbolTable;
class VarDeclaration;

namespace dsl {
class DSLCore;
class DSLFunction;
} // namespace dsl

enum class VariableStorage : int8_t {
    kGlobal,
    kInterfaceBlock,
    kLocal,
    kParameter
};

/**
 * Represents a variable, whether local, global, or a function parameter. This represents the
 * variable itself (the storage location), which is shared between all VariableReferences which
 * read or write that storage location.
 */
class Variable final : public Symbol {
public:
    using Storage = VariableStorage;

    inline static constexpr Kind kSymbolKind = Kind::kVariable;

    Variable(Position pos, Position modifiersPosition, const Modifiers* modifiers,
            std::string_view name, const Type* type, bool builtin, Storage storage)
    : INHERITED(pos, kSymbolKind, name, type)
    , fModifiersPosition(modifiersPosition)
    , fModifiers(modifiers)
    , fStorage(storage)
    , fBuiltin(builtin) {}

    ~Variable() override;

    static std::unique_ptr<Variable> Convert(const Context& context, Position pos,
            Position modifiersPos, const Modifiers& modifiers, const Type* baseType,
            Position namePos, std::string_view name, bool isArray,
            std::unique_ptr<Expression> arraySize, Variable::Storage storage);

    static std::unique_ptr<Variable> Make(const Context& context, Position pos,
            Position modifiersPos, const Modifiers& modifiers, const Type* baseType,
            std::string_view name, bool isArray, std::unique_ptr<Expression> arraySize,
            Variable::Storage storage);

    /**
     * Creates a local scratch variable and the associated VarDeclaration statement.
     * Useful when doing IR rewrites, e.g. inlining a function call.
     */
    struct ScratchVariable {
        const Variable* fVarSymbol;
        std::unique_ptr<Statement> fVarDecl;
    };
    static ScratchVariable MakeScratchVariable(const Context& context,
                                               std::string_view baseName,
                                               const Type* type,
                                               const Modifiers& modifiers,
                                               SymbolTable* symbolTable,
                                               std::unique_ptr<Expression> initialValue);
    const Modifiers& modifiers() const {
        return *fModifiers;
    }

    void setModifiers(const Modifiers* modifiers) {
        fModifiers = modifiers;
    }

    Position modifiersPosition() const {
        return fModifiersPosition;
    }

    bool isBuiltin() const {
        return fBuiltin;
    }

    Storage storage() const {
        return (Storage) fStorage;
    }

    const Expression* initialValue() const;

    void setDeclaration(VarDeclaration* declaration) {
        SkASSERT(!fDeclaration);
        fDeclaration = declaration;
    }

    void detachDeadVarDeclaration() const {
        // The VarDeclaration is being deleted, so our reference to it has become stale.
        // This variable is now dead, so it shouldn't matter that we are modifying its symbol.
        const_cast<Variable*>(this)->fDeclaration = nullptr;
    }

    std::string description() const override {
        return this->modifiers().description() + this->type().displayName() + " " +
               std::string(this->name());
    }

private:
    VarDeclaration* fDeclaration = nullptr;
    // We don't store the position in the Modifiers object itself because they are pooled
    Position fModifiersPosition;
    const Modifiers* fModifiers;
    VariableStorage fStorage;
    bool fBuiltin;

    using INHERITED = Symbol;

    friend class dsl::DSLCore;
    friend class dsl::DSLFunction;
    friend class VariableReference;
};

} // namespace SkSL

#endif
