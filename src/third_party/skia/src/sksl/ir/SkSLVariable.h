/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_VARIABLE
#define SKSL_VARIABLE

#include "src/sksl/SkSLPosition.h"
#include "src/sksl/ir/SkSLModifiers.h"
#include "src/sksl/ir/SkSLSymbol.h"
#include "src/sksl/ir/SkSLType.h"
#include "src/sksl/ir/SkSLVariableReference.h"

namespace SkSL {

class Expression;

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

    static constexpr Kind kSymbolKind = Kind::kVariable;

    Variable(int offset, ModifiersPool::Handle modifiers, StringFragment name, const Type* type,
             bool builtin, Storage storage, const Expression* initialValue = nullptr)
    : INHERITED(offset, kSymbolKind, name, type)
    , fInitialValue(initialValue)
    , fModifiersHandle(modifiers)
    , fStorage(storage)
    , fBuiltin(builtin) {}

    const Modifiers& modifiers() const {
        return *fModifiersHandle;
    }

    const ModifiersPool::Handle& modifiersHandle() const {
        return fModifiersHandle;
    }

    bool isBuiltin() const {
        return fBuiltin;
    }

    Storage storage() const {
        return (Storage) fStorage;
    }

    const Expression* initialValue() const {
        return fInitialValue;
    }

    void setInitialValue(const Expression* initialValue) {
        SkASSERT(!this->initialValue());
        fInitialValue = initialValue;
    }

    String description() const override {
        return this->modifiers().description() + this->type().name() + " " + this->name();
    }

private:
    const Expression* fInitialValue = nullptr;
    ModifiersPool::Handle fModifiersHandle;
    VariableStorage fStorage;
    bool fBuiltin;

    using INHERITED = Symbol;

    friend class VariableReference;
};

} // namespace SkSL

#endif
