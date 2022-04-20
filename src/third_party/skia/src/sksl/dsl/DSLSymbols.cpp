/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/sksl/DSLSymbols.h"

#include "include/private/SkSLSymbol.h"
#include "include/sksl/SkSLPosition.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLThreadContext.h"
#include "src/sksl/dsl/priv/DSLWriter.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLType.h"
#include "src/sksl/ir/SkSLVariable.h"

#include <string>
#include <type_traits>
#include <utility>

namespace SkSL {

namespace dsl {

class DSLVarBase;

static bool is_type_in_symbol_table(std::string_view name, SkSL::SymbolTable* symbols) {
    const SkSL::Symbol* s = (*symbols)[name];
    return s && s->is<Type>();
}

void PushSymbolTable() {
    SymbolTable::Push(&ThreadContext::SymbolTable());
}

void PopSymbolTable() {
    SymbolTable::Pop(&ThreadContext::SymbolTable());
}

std::shared_ptr<SymbolTable> CurrentSymbolTable() {
    return ThreadContext::SymbolTable();
}

DSLExpression Symbol(std::string_view name, Position pos) {
    return DSLExpression(ThreadContext::Compiler().convertIdentifier(pos, name), pos);
}

bool IsType(std::string_view name) {
    return is_type_in_symbol_table(name, CurrentSymbolTable().get());
}

bool IsBuiltinType(std::string_view name) {
    return is_type_in_symbol_table(name, CurrentSymbolTable()->builtinParent());
}

void AddToSymbolTable(DSLVarBase& var, Position pos) {
    const SkSL::Variable* skslVar = DSLWriter::Var(var);
    if (skslVar) {
        CurrentSymbolTable()->addWithoutOwnership(skslVar);
    }
    ThreadContext::ReportErrors(pos);
}

const std::string* Retain(std::string string) {
    return CurrentSymbolTable()->takeOwnershipOfString(std::move(string));
}

} // namespace dsl

} // namespace SkSL
