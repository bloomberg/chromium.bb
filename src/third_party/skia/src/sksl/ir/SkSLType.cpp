/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLType.h"

#include "src/sksl/SkSLConstantFolder.h"
#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLConstructor.h"
#include "src/sksl/ir/SkSLConstructorArrayCast.h"
#include "src/sksl/ir/SkSLConstructorCompoundCast.h"
#include "src/sksl/ir/SkSLConstructorScalarCast.h"
#include "src/sksl/ir/SkSLExternalFunctionReference.h"
#include "src/sksl/ir/SkSLFunctionReference.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLType.h"
#include "src/sksl/ir/SkSLTypeReference.h"

namespace SkSL {

static constexpr int kMaxStructDepth = 8;

class ArrayType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kArray;

    ArrayType(skstd::string_view name, const char* abbrev, const Type& componentType, int count)
        : INHERITED(name, abbrev, kTypeKind)
        , fComponentType(componentType)
        , fCount(count) {
        // Only allow explicitly-sized arrays.
        SkASSERT(count > 0);
        // Disallow multi-dimensional arrays.
        SkASSERT(!componentType.is<ArrayType>());
    }

    bool isArray() const override {
        return true;
    }

    const Type& componentType() const override {
        return fComponentType;
    }

    int columns() const override {
        return fCount;
    }

    int bitWidth() const override {
        return this->componentType().bitWidth();
    }

    bool allowedInES2() const override {
        return fComponentType.allowedInES2();
    }

private:
    using INHERITED = Type;

    const Type& fComponentType;
    int fCount;
};

class GenericType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kGeneric;

    GenericType(const char* name, std::vector<const Type*> coercibleTypes)
        : INHERITED(name, "G", kTypeKind)
        , fCoercibleTypes(std::move(coercibleTypes)) {}

    const std::vector<const Type*>& coercibleTypes() const override {
        return fCoercibleTypes;
    }

private:
    using INHERITED = Type;

    std::vector<const Type*> fCoercibleTypes;
};

class LiteralType : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kLiteral;

    LiteralType(const char* name, const Type& scalarType, int8_t priority)
        : INHERITED(name, "L", kTypeKind)
        , fScalarType(scalarType)
        , fPriority(priority) {}

    const Type& scalarTypeForLiteral() const override {
        return fScalarType;
    }

    int priority() const override {
        return fPriority;
    }

    int columns() const override {
        return 1;
    }

    int rows() const override {
        return 1;
    }

    NumberKind numberKind() const override {
        return fScalarType.numberKind();
    }

    int bitWidth() const override {
        return fScalarType.bitWidth();
    }

    bool isScalar() const override {
        return true;
    }

    bool isLiteral() const override {
        return true;
    }

private:
    using INHERITED = Type;

    const Type& fScalarType;
    int8_t fPriority;
};


class ScalarType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kScalar;

    ScalarType(skstd::string_view name, const char* abbrev, NumberKind numberKind, int8_t priority,
               int8_t bitWidth)
        : INHERITED(name, abbrev, kTypeKind)
        , fNumberKind(numberKind)
        , fPriority(priority)
        , fBitWidth(bitWidth) {}

    NumberKind numberKind() const override {
        return fNumberKind;
    }

    int priority() const override {
        return fPriority;
    }

    int bitWidth() const override {
        return fBitWidth;
    }

    int columns() const override {
        return 1;
    }

    int rows() const override {
        return 1;
    }

    bool isScalar() const override {
        return true;
    }

    bool allowedInES2() const override {
        return fNumberKind != NumberKind::kUnsigned;
    }

private:
    using INHERITED = Type;

    NumberKind fNumberKind;
    int8_t fPriority;
    int8_t fBitWidth;
};

class MatrixType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kMatrix;

    MatrixType(skstd::string_view name, const char* abbrev, const Type& componentType,
               int8_t columns, int8_t rows)
        : INHERITED(name, abbrev, kTypeKind)
        , fComponentType(componentType.as<ScalarType>())
        , fColumns(columns)
        , fRows(rows) {
        SkASSERT(columns >= 2 && columns <= 4);
        SkASSERT(rows >= 2 && rows <= 4);
    }

    const ScalarType& componentType() const override {
        return fComponentType;
    }

    int columns() const override {
        return fColumns;
    }

    int rows() const override {
        return fRows;
    }

    int bitWidth() const override {
        return this->componentType().bitWidth();
    }

    bool isMatrix() const override {
        return true;
    }

    bool allowedInES2() const override {
        return fColumns == fRows;
    }

private:
    using INHERITED = Type;

    const ScalarType& fComponentType;
    int8_t fColumns;
    int8_t fRows;
};

class TextureType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kTexture;

    TextureType(const char* name, SpvDim_ dimensions, bool isDepth, bool isArrayed,
                bool isMultisampled, bool isSampled)
        : INHERITED(name, "T", kTypeKind)
        , fDimensions(dimensions)
        , fIsDepth(isDepth)
        , fIsArrayed(isArrayed)
        , fIsMultisampled(isMultisampled)
        , fIsSampled(isSampled) {}

    SpvDim_ dimensions() const override {
        return fDimensions;
    }

    bool isDepth() const override {
        return fIsDepth;
    }

    bool isArrayedTexture() const override {
        return fIsArrayed;
    }

    bool isMultisampled() const override {
        return fIsMultisampled;
    }

    bool isSampled() const override {
        return fIsSampled;
    }

private:
    using INHERITED = Type;

    SpvDim_ fDimensions;
    bool fIsDepth;
    bool fIsArrayed;
    bool fIsMultisampled;
    bool fIsSampled;
};

class SamplerType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kSampler;

    SamplerType(const char* name, const Type& textureType)
        : INHERITED(name, "Z", kTypeKind)
        , fTextureType(textureType.as<TextureType>()) {}

    const TextureType& textureType() const override {
        return fTextureType;
    }

    SpvDim_ dimensions() const override {
        return fTextureType.dimensions();
    }

    bool isDepth() const override {
        return fTextureType.isDepth();
    }

    bool isArrayedTexture() const override {
        return fTextureType.isArrayedTexture();
    }

    bool isMultisampled() const override {
        return fTextureType.isMultisampled();
    }

    bool isSampled() const override {
        return fTextureType.isSampled();
    }

private:
    using INHERITED = Type;

    const TextureType& fTextureType;
};

class StructType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kStruct;

    StructType(int offset, skstd::string_view name, std::vector<Field> fields)
        : INHERITED(std::move(name), "S", kTypeKind, offset)
        , fFields(std::move(fields)) {}

    const std::vector<Field>& fields() const override {
        return fFields;
    }

    bool isStruct() const override {
        return true;
    }

    bool allowedInES2() const override {
        return std::all_of(fFields.begin(), fFields.end(), [](const Field& f) {
            return f.fType->allowedInES2();
        });
    }

private:
    using INHERITED = Type;

    std::vector<Field> fFields;
};

class VectorType final : public Type {
public:
    static constexpr TypeKind kTypeKind = TypeKind::kVector;

    VectorType(skstd::string_view name, const char* abbrev, const Type& componentType,
               int8_t columns)
        : INHERITED(name, abbrev, kTypeKind)
        , fComponentType(componentType.as<ScalarType>())
        , fColumns(columns) {
        SkASSERT(columns >= 2 && columns <= 4);
    }

    const ScalarType& componentType() const override {
        return fComponentType;
    }

    int columns() const override {
        return fColumns;
    }

    int rows() const override {
        return 1;
    }

    int bitWidth() const override {
        return this->componentType().bitWidth();
    }

    bool isVector() const override {
        return true;
    }

    bool allowedInES2() const override {
        return fComponentType.allowedInES2();
    }

private:
    using INHERITED = Type;

    const ScalarType& fComponentType;
    int8_t fColumns;
};

String Type::getArrayName(int arraySize) const {
    skstd::string_view name = this->name();
    return String::printf("%.*s[%d]", (int)name.size(), name.data(), arraySize);
}

std::unique_ptr<Type> Type::MakeArrayType(skstd::string_view name, const Type& componentType,
                                          int columns) {
    return std::make_unique<ArrayType>(std::move(name), componentType.abbreviatedName(),
                                       componentType, columns);
}

std::unique_ptr<Type> Type::MakeGenericType(const char* name, std::vector<const Type*> types) {
    return std::make_unique<GenericType>(name, std::move(types));
}

std::unique_ptr<Type> Type::MakeLiteralType(const char* name, const Type& scalarType,
                                            int8_t priority) {
    return std::make_unique<LiteralType>(name, scalarType, priority);
}

std::unique_ptr<Type> Type::MakeMatrixType(skstd::string_view name, const char* abbrev,
                                           const Type& componentType, int columns, int8_t rows) {
    return std::make_unique<MatrixType>(name, abbrev, componentType, columns, rows);
}

std::unique_ptr<Type> Type::MakeSamplerType(const char* name, const Type& textureType) {
    return std::make_unique<SamplerType>(name, textureType);
}

std::unique_ptr<Type> Type::MakeSpecialType(const char* name, const char* abbrev,
                                            Type::TypeKind typeKind) {
    return std::unique_ptr<Type>(new Type(name, abbrev, typeKind));
}

std::unique_ptr<Type> Type::MakeScalarType(skstd::string_view name, const char* abbrev,
                                           Type::NumberKind numberKind, int8_t priority,
                                           int8_t bitWidth) {
    return std::make_unique<ScalarType>(name, abbrev, numberKind, priority, bitWidth);

}

std::unique_ptr<Type> Type::MakeStructType(int offset, skstd::string_view name,
                                           std::vector<Field> fields) {
    return std::make_unique<StructType>(offset, name, std::move(fields));
}

std::unique_ptr<Type> Type::MakeTextureType(const char* name, SpvDim_ dimensions, bool isDepth,
                                            bool isArrayedTexture, bool isMultisampled,
                                            bool isSampled) {
    return std::make_unique<TextureType>(name, dimensions, isDepth, isArrayedTexture,
                                         isMultisampled, isSampled);
}

std::unique_ptr<Type> Type::MakeVectorType(skstd::string_view name, const char* abbrev,
                                           const Type& componentType, int columns) {
    return std::make_unique<VectorType>(name, abbrev, componentType, columns);
}

CoercionCost Type::coercionCost(const Type& other) const {
    if (*this == other) {
        return CoercionCost::Free();
    }
    if (this->typeKind() == other.typeKind() &&
        (this->isVector() || this->isMatrix() || this->isArray())) {
        // Vectors/matrices/arrays of the same size can be coerced if their component type can be.
        if (this->isMatrix() && (this->rows() != other.rows())) {
            return CoercionCost::Impossible();
        }
        if (this->columns() != other.columns()) {
            return CoercionCost::Impossible();
        }
        return this->componentType().coercionCost(other.componentType());
    }
    if (this->isNumber() && other.isNumber()) {
        if (this->isLiteral() && this->isInteger()) {
            return CoercionCost::Free();
        } else if (this->numberKind() != other.numberKind()) {
            return CoercionCost::Impossible();
        } else if (other.priority() >= this->priority()) {
            return CoercionCost::Normal(other.priority() - this->priority());
        } else {
            return CoercionCost::Narrowing(this->priority() - other.priority());
        }
    }
    if (fTypeKind == TypeKind::kGeneric) {
        const std::vector<const Type*>& types = this->coercibleTypes();
        for (size_t i = 0; i < types.size(); i++) {
            if (*types[i] == other) {
                return CoercionCost::Normal((int) i + 1);
            }
        }
    }
    return CoercionCost::Impossible();
}

const Type* Type::applyPrecisionQualifiers(const Context& context,
                                           const Modifiers& modifiers,
                                           SymbolTable* symbols,
                                           int offset) const {
    // SkSL doesn't support low precision, so `lowp` is interpreted as medium precision.
    bool highp   = modifiers.fFlags & Modifiers::kHighp_Flag;
    bool mediump = modifiers.fFlags & Modifiers::kMediump_Flag;
    bool lowp    = modifiers.fFlags & Modifiers::kLowp_Flag;

    if (!lowp && !mediump && !highp) {
        // No precision qualifiers here. Return the type as-is.
        return this;
    }

    if (!ProgramConfig::IsRuntimeEffect(context.fConfig->fKind)) {
        // We want to discourage precision modifiers internally. Instead, use the type that
        // corresponds to the precision you need. (e.g. half vs float, short vs int)
        context.fErrors->error(offset, "precision qualifiers are not allowed");
        return nullptr;
    }

    if ((int(lowp) + int(mediump) + int(highp)) != 1) {
        context.fErrors->error(offset, "only one precision qualifier can be used");
        return nullptr;
    }

    const Type& component = this->componentType();
    if (component.highPrecision()) {
        if (highp) {
            // Type is already high precision, and we are requesting high precision. Return as-is.
            return this;
        }

        // Ascertain the mediump equivalent type for this type, if any.
        const Type* mediumpType;
        switch (component.numberKind()) {
            case Type::NumberKind::kFloat:
                mediumpType = context.fTypes.fHalf.get();
                break;

            case Type::NumberKind::kSigned:
                mediumpType = context.fTypes.fShort.get();
                break;

            case Type::NumberKind::kUnsigned:
                mediumpType = context.fTypes.fUShort.get();
                break;

            default:
                mediumpType = nullptr;
                break;
        }

        if (mediumpType) {
            // Convert the mediump component type into the final vector/matrix/array type as needed.
            return this->isArray()
                           ? symbols->addArrayDimension(mediumpType, this->columns())
                           : &mediumpType->toCompound(context, this->columns(), this->rows());
        }
    }

    context.fErrors->error(offset, "type '" + this->displayName() +
                                   "' does not support precision qualifiers");
    return nullptr;
}

const Type& Type::toCompound(const Context& context, int columns, int rows) const {
    SkASSERT(this->isScalar());
    if (columns == 1 && rows == 1) {
        return *this;
    }
    if (*this == *context.fTypes.fFloat || *this == *context.fTypes.fFloatLiteral) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fFloat;
                    case 2: return *context.fTypes.fFloat2;
                    case 3: return *context.fTypes.fFloat3;
                    case 4: return *context.fTypes.fFloat4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            case 2:
                switch (columns) {
                    case 2: return *context.fTypes.fFloat2x2;
                    case 3: return *context.fTypes.fFloat3x2;
                    case 4: return *context.fTypes.fFloat4x2;
                    default: SK_ABORT("unsupported matrix column count (%d)", columns);
                }
            case 3:
                switch (columns) {
                    case 2: return *context.fTypes.fFloat2x3;
                    case 3: return *context.fTypes.fFloat3x3;
                    case 4: return *context.fTypes.fFloat4x3;
                    default: SK_ABORT("unsupported matrix column count (%d)", columns);
                }
            case 4:
                switch (columns) {
                    case 2: return *context.fTypes.fFloat2x4;
                    case 3: return *context.fTypes.fFloat3x4;
                    case 4: return *context.fTypes.fFloat4x4;
                    default: SK_ABORT("unsupported matrix column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    } else if (*this == *context.fTypes.fHalf) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fHalf;
                    case 2: return *context.fTypes.fHalf2;
                    case 3: return *context.fTypes.fHalf3;
                    case 4: return *context.fTypes.fHalf4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            case 2:
                switch (columns) {
                    case 2: return *context.fTypes.fHalf2x2;
                    case 3: return *context.fTypes.fHalf3x2;
                    case 4: return *context.fTypes.fHalf4x2;
                    default: SK_ABORT("unsupported matrix column count (%d)", columns);
                }
            case 3:
                switch (columns) {
                    case 2: return *context.fTypes.fHalf2x3;
                    case 3: return *context.fTypes.fHalf3x3;
                    case 4: return *context.fTypes.fHalf4x3;
                    default: SK_ABORT("unsupported matrix column count (%d)", columns);
                }
            case 4:
                switch (columns) {
                    case 2: return *context.fTypes.fHalf2x4;
                    case 3: return *context.fTypes.fHalf3x4;
                    case 4: return *context.fTypes.fHalf4x4;
                    default: SK_ABORT("unsupported matrix column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    } else if (*this == *context.fTypes.fInt || *this == *context.fTypes.fIntLiteral) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fInt;
                    case 2: return *context.fTypes.fInt2;
                    case 3: return *context.fTypes.fInt3;
                    case 4: return *context.fTypes.fInt4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    } else if (*this == *context.fTypes.fShort) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fShort;
                    case 2: return *context.fTypes.fShort2;
                    case 3: return *context.fTypes.fShort3;
                    case 4: return *context.fTypes.fShort4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    } else if (*this == *context.fTypes.fUInt) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fUInt;
                    case 2: return *context.fTypes.fUInt2;
                    case 3: return *context.fTypes.fUInt3;
                    case 4: return *context.fTypes.fUInt4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    } else if (*this == *context.fTypes.fUShort) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fUShort;
                    case 2: return *context.fTypes.fUShort2;
                    case 3: return *context.fTypes.fUShort3;
                    case 4: return *context.fTypes.fUShort4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    } else if (*this == *context.fTypes.fBool) {
        switch (rows) {
            case 1:
                switch (columns) {
                    case 1: return *context.fTypes.fBool;
                    case 2: return *context.fTypes.fBool2;
                    case 3: return *context.fTypes.fBool3;
                    case 4: return *context.fTypes.fBool4;
                    default: SK_ABORT("unsupported vector column count (%d)", columns);
                }
            default: SK_ABORT("unsupported row count (%d)", rows);
        }
    }
    SkDEBUGFAILF("unsupported toCompound type %s", this->description().c_str());
    return *context.fTypes.fVoid;
}

const Type* Type::clone(SymbolTable* symbolTable) const {
    // Many types are built-ins, and exist in every SymbolTable by default.
    if (this->isInBuiltinTypes()) {
        return this;
    }
    // Even if the type isn't a built-in, it might already exist in the SymbolTable.
    const Symbol* clonedSymbol = (*symbolTable)[this->name()];
    if (clonedSymbol != nullptr) {
        const Type& clonedType = clonedSymbol->as<Type>();
        SkASSERT(clonedType.typeKind() == this->typeKind());
        return &clonedType;
    }
    // This type actually needs to be cloned into the destination SymbolTable.
    switch (this->typeKind()) {
        case TypeKind::kArray: {
            return symbolTable->addArrayDimension(&this->componentType(), this->columns());
        }
        case TypeKind::kStruct: {
            const String* name = symbolTable->takeOwnershipOfString(String(this->name()));
            return symbolTable->add(Type::MakeStructType(this->fOffset, *name, this->fields()));
        }
        default:
            SkDEBUGFAILF("don't know how to clone type '%s'", this->description().c_str());
            return nullptr;
    }
}

std::unique_ptr<Expression> Type::coerceExpression(std::unique_ptr<Expression> expr,
                                                   const Context& context) const {
    if (!expr) {
        return nullptr;
    }
    const int offset = expr->fOffset;
    if (expr->is<FunctionReference>() || expr->is<ExternalFunctionReference>()) {
        context.fErrors->error(offset, "expected '(' to begin function call");
        return nullptr;
    }
    if (expr->is<TypeReference>()) {
        context.fErrors->error(offset, "expected '(' to begin constructor invocation");
        return nullptr;
    }
    if (expr->type() == *this) {
        return expr;
    }

    const Program::Settings& settings = context.fConfig->fSettings;
    if (!expr->coercionCost(*this).isPossible(settings.fAllowNarrowingConversions)) {
        context.fErrors->error(offset, "expected '" + this->displayName() + "', but found '" +
                                       expr->type().displayName() + "'");
        return nullptr;
    }

    if (this->isScalar()) {
        return ConstructorScalarCast::Make(context, offset, *this, std::move(expr));
    }
    if (this->isVector() || this->isMatrix()) {
        return ConstructorCompoundCast::Make(context, offset, *this, std::move(expr));
    }
    if (this->isArray()) {
        return ConstructorArrayCast::Make(context, offset, *this, std::move(expr));
    }
    context.fErrors->error(offset, "cannot construct '" + this->displayName() + "'");
    return nullptr;
}

bool Type::isOrContainsArray() const {
    if (this->isStruct()) {
        for (const Field& f : this->fields()) {
            if (f.fType->isOrContainsArray()) {
                return true;
            }
        }
        return false;
    }

    return this->isArray();
}


bool Type::containsPrivateFields() const {
    if (this->isPrivate()) {
        return true;
    }
    if (this->isStruct()) {
        for (const auto& f : this->fields()) {
            if (f.fType->containsPrivateFields()) {
                return true;
            }
        }
    }
    return false;
}

bool Type::isTooDeeplyNested(int limit) const {
    if (limit < 0) {
        return true;
    }

    if (this->isStruct()) {
        for (const Type::Field& f : this->fields()) {
            if (f.fType->isTooDeeplyNested(limit - 1)) {
                return true;
            }
        }
    }

    return false;
}

bool Type::isTooDeeplyNested() const {
    return this->isTooDeeplyNested(kMaxStructDepth);
}

bool Type::checkForOutOfRangeLiteral(const Context& context, const Expression& expr) const {
    bool foundError = false;
    const Type& baseType = this->componentType();
    if (baseType.isInteger()) {
        // Replace constant expressions with their corresponding values.
        const Expression* valueExpr = ConstantFolder::GetConstantValueForVariable(expr);
        if (valueExpr->allowsConstantSubexpressions()) {
            // Iterate over every constant subexpression in the value.
            int numSlots = valueExpr->type().slotCount();
            for (int slot = 0; slot < numSlots; ++slot) {
                const Expression* subexpr = valueExpr->getConstantSubexpression(slot);
                if (!subexpr || !subexpr->is<IntLiteral>()) {
                    continue;
                }
                // Look for an IntLiteral value that is out of range for the corresponding type.
                SKSL_INT value = subexpr->as<IntLiteral>().value();
                if (value < baseType.minimumValue() || value > baseType.maximumValue()) {
                    // We found a value that can't fit in the type. Flag it as an error.
                    context.fErrors->error(expr.fOffset,
                                           String("integer is out of range for type '") +
                                           this->displayName().c_str() + "': " + to_string(value));
                    foundError = true;
                }
            }
        }
    }

    // We don't need range checks for floats or booleans; any matched-type value is acceptable.
    return foundError;
}

SKSL_INT Type::convertArraySize(const Context& context, std::unique_ptr<Expression> size) const {
    size = context.fTypes.fInt->coerceExpression(std::move(size), context);
    if (!size) {
        return 0;
    }
    if (this->isArray()) {
        context.fErrors->error(size->fOffset, "multi-dimensional arrays are not supported");
        return 0;
    }
    if (this->isVoid()) {
        context.fErrors->error(size->fOffset, "type 'void' may not be used in an array");
        return 0;
    }
    if (this->isOpaque()) {
        context.fErrors->error(size->fOffset, "opaque type '" + this->name() +
                                              "' may not be used in an array");
        return 0;
    }
    if (!size->is<IntLiteral>()) {
        context.fErrors->error(size->fOffset, "array size must be an integer");
        return 0;
    }
    SKSL_INT count = size->as<IntLiteral>().value();
    if (count <= 0) {
        context.fErrors->error(size->fOffset, "array size must be positive");
        return 0;
    }
    if (!SkTFitsIn<int32_t>(count)) {
        context.fErrors->error(size->fOffset, "array size is too large");
        return 0;
    }
    return static_cast<int>(count);
}

}  // namespace SkSL
