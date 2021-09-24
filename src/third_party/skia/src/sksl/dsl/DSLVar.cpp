/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/sksl/DSLVar.h"

#include "include/sksl/DSLModifiers.h"
#include "include/sksl/DSLType.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/dsl/priv/DSLWriter.h"
#include "src/sksl/ir/SkSLBinaryExpression.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLVariable.h"
#include "src/sksl/ir/SkSLVariableReference.h"

namespace SkSL {

namespace dsl {

DSLVarBase::DSLVarBase(DSLType type, skstd::string_view name, DSLExpression initialValue,
                       PositionInfo pos)
    : DSLVarBase(DSLModifiers(), std::move(type), name, std::move(initialValue), pos) {}

DSLVarBase::DSLVarBase(DSLType type, DSLExpression initialValue, PositionInfo pos)
    : DSLVarBase(type, "var", std::move(initialValue), pos) {}

DSLVarBase::DSLVarBase(const DSLModifiers& modifiers, DSLType type, DSLExpression initialValue,
                       PositionInfo pos)
    : DSLVarBase(modifiers, type, "var", std::move(initialValue), pos) {}

DSLVarBase::DSLVarBase(const DSLModifiers& modifiers, DSLType type, skstd::string_view name,
                       DSLExpression initialValue, PositionInfo pos)
    : fModifiers(std::move(modifiers))
    , fType(std::move(type))
    , fRawName(name)
    , fName(fType.skslType().isOpaque() ? name : DSLWriter::Name(name))
    , fInitialValue(std::move(initialValue))
    , fDeclared(DSLWriter::MarkVarsDeclared())
    , fPosition(pos) {
    if (fModifiers.fModifiers.fFlags & Modifiers::kUniform_Flag) {
#if SK_SUPPORT_GPU && !defined(SKSL_STANDALONE)
        if (DSLWriter::InFragmentProcessor()) {
            const SkSL::Type& skslType = type.skslType();
            GrSLType grslType;
            int count;
            if (skslType.isArray()) {
                SkAssertResult(SkSL::type_to_grsltype(DSLWriter::Context(),
                                                      skslType.componentType(),
                                                      &grslType));
                count = skslType.columns();
                SkASSERT(count > 0);
            } else {
                SkAssertResult(SkSL::type_to_grsltype(DSLWriter::Context(), skslType,
                                                      &grslType));
                count = 0;
            }
            const char* uniformName;
            SkASSERT(DSLWriter::CurrentEmitArgs());
            fUniformHandle = DSLWriter::CurrentEmitArgs()->fUniformHandler->addUniformArray(
                                                                 &DSLWriter::CurrentEmitArgs()->fFp,
                                                                 kFragment_GrShaderFlag,
                                                                 grslType,
                                                                 String(this->name()).c_str(),
                                                                 count,
                                                                 &uniformName).toIndex();
            fName = uniformName;
        }
#endif // SK_SUPPORT_GPU && !defined(SKSL_STANDALONE)
    }
}

DSLVarBase::~DSLVarBase() {
    if (fDeclaration && !fDeclared) {
        DSLWriter::ReportError(String::printf("variable '%.*s' was destroyed without being "
                                              "declared",
                                              (int)fRawName.length(),
                                              fRawName.data()).c_str());
    }
}

void DSLVarBase::swap(DSLVarBase& other) {
    SkASSERT(this->storage() == other.storage());
    std::swap(fModifiers, other.fModifiers);
    std::swap(fType, other.fType);
    std::swap(fUniformHandle, other.fUniformHandle);
    std::swap(fDeclaration, other.fDeclaration);
    std::swap(fVar, other.fVar);
    std::swap(fRawName, other.fRawName);
    std::swap(fName, other.fName);
    std::swap(fInitialValue.fExpression, other.fInitialValue.fExpression);
    std::swap(fDeclared, other.fDeclared);
    std::swap(fInitialized, other.fInitialized);
    std::swap(fPosition, other.fPosition);
}

void DSLVar::swap(DSLVar& other) {
    INHERITED::swap(other);
}

VariableStorage DSLVar::storage() const {
    return VariableStorage::kLocal;
}

DSLGlobalVar::DSLGlobalVar(const char* name)
    : INHERITED(kVoid_Type, name, DSLExpression(), PositionInfo()) {
    fName = name;
    DSLWriter::MarkDeclared(*this);
#if SK_SUPPORT_GPU && !defined(SKSL_STANDALONE)
    if (!strcmp(name, "sk_SampleCoord")) {
        fName = DSLWriter::CurrentEmitArgs()->fSampleCoord;
        // The actual sk_SampleCoord variable hasn't been created by GrGLSLFPFragmentBuilder yet, so
        // if we attempt to look it up in the symbol table we'll get null. As we are currently
        // converting all DSL code into strings rather than nodes, all we really need is a
        // correctly-named variable with the right type, so we just create a placeholder for it.
        // TODO(skia/11330): we'll need to fix this when switching over to nodes.
        const SkSL::Modifiers* modifiers = DSLWriter::Context().fModifiersPool->add(
                SkSL::Modifiers(SkSL::Layout(/*flags=*/0, /*location=*/-1, /*offset=*/-1,
                                             /*binding=*/-1, /*index=*/-1, /*set=*/-1,
                                             SK_MAIN_COORDS_BUILTIN, /*inputAttachmentIndex=*/-1),
                                SkSL::Modifiers::kNo_Flag));

        fVar = DSLWriter::SymbolTable()->takeOwnershipOfIRNode(std::make_unique<SkSL::Variable>(
                /*offset=*/-1,
                modifiers,
                fName,
                DSLWriter::Context().fTypes.fFloat2.get(),
                /*builtin=*/true,
                SkSL::VariableStorage::kGlobal));
        fInitialized = true;
        return;
    }
#endif
    const SkSL::Symbol* result = (*DSLWriter::SymbolTable())[fName];
    SkASSERTF(result, "could not find '%.*s' in symbol table", (int)fName.length(), fName.data());
    fVar = &result->as<SkSL::Variable>();
    fInitialized = true;
}

void DSLGlobalVar::swap(DSLGlobalVar& other) {
    INHERITED::swap(other);
}

VariableStorage DSLGlobalVar::storage() const {
    return VariableStorage::kGlobal;
}

void DSLParameter::swap(DSLParameter& other) {
    INHERITED::swap(other);
}

VariableStorage DSLParameter::storage() const {
    return VariableStorage::kParameter;
}


DSLPossibleExpression DSLVarBase::operator[](DSLExpression&& index) {
    return DSLExpression(*this)[std::move(index)];
}

DSLPossibleExpression DSLVarBase::assign(DSLExpression expr) {
    return DSLWriter::ConvertBinary(DSLExpression(*this).release(), SkSL::Token::Kind::TK_EQ,
                                    expr.release());
}

DSLPossibleExpression DSLVar::operator=(DSLExpression expr) {
    return this->assign(std::move(expr));
}

DSLPossibleExpression DSLGlobalVar::operator=(DSLExpression expr) {
    return this->assign(std::move(expr));
}

DSLPossibleExpression DSLParameter::operator=(DSLExpression expr) {
    return this->assign(std::move(expr));
}

} // namespace dsl

} // namespace SkSL
