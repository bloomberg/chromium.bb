/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLIRGenerator.h"

#include "limits.h"
#include <iterator>
#include <memory>
#include <unordered_set>

#include "include/private/SkSLLayout.h"
#include "include/private/SkTArray.h"
#include "include/sksl/DSLCore.h"
#include "src/core/SkScopeExit.h"
#include "src/sksl/SkSLAnalysis.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLConstantFolder.h"
#include "src/sksl/SkSLOperators.h"
#include "src/sksl/SkSLParser.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/ir/SkSLBinaryExpression.h"
#include "src/sksl/ir/SkSLBoolLiteral.h"
#include "src/sksl/ir/SkSLBreakStatement.h"
#include "src/sksl/ir/SkSLConstructor.h"
#include "src/sksl/ir/SkSLContinueStatement.h"
#include "src/sksl/ir/SkSLDiscardStatement.h"
#include "src/sksl/ir/SkSLDoStatement.h"
#include "src/sksl/ir/SkSLExpressionStatement.h"
#include "src/sksl/ir/SkSLExternalFunctionCall.h"
#include "src/sksl/ir/SkSLExternalFunctionReference.h"
#include "src/sksl/ir/SkSLField.h"
#include "src/sksl/ir/SkSLFieldAccess.h"
#include "src/sksl/ir/SkSLFloatLiteral.h"
#include "src/sksl/ir/SkSLForStatement.h"
#include "src/sksl/ir/SkSLFunctionCall.h"
#include "src/sksl/ir/SkSLFunctionDeclaration.h"
#include "src/sksl/ir/SkSLFunctionDefinition.h"
#include "src/sksl/ir/SkSLFunctionPrototype.h"
#include "src/sksl/ir/SkSLFunctionReference.h"
#include "src/sksl/ir/SkSLIfStatement.h"
#include "src/sksl/ir/SkSLIndexExpression.h"
#include "src/sksl/ir/SkSLIntLiteral.h"
#include "src/sksl/ir/SkSLInterfaceBlock.h"
#include "src/sksl/ir/SkSLMethodReference.h"
#include "src/sksl/ir/SkSLNop.h"
#include "src/sksl/ir/SkSLPoison.h"
#include "src/sksl/ir/SkSLPostfixExpression.h"
#include "src/sksl/ir/SkSLPrefixExpression.h"
#include "src/sksl/ir/SkSLReturnStatement.h"
#include "src/sksl/ir/SkSLSetting.h"
#include "src/sksl/ir/SkSLStructDefinition.h"
#include "src/sksl/ir/SkSLSwitchCase.h"
#include "src/sksl/ir/SkSLSwitchStatement.h"
#include "src/sksl/ir/SkSLSwizzle.h"
#include "src/sksl/ir/SkSLTernaryExpression.h"
#include "src/sksl/ir/SkSLUnresolvedFunction.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"
#include "src/sksl/ir/SkSLVariable.h"
#include "src/sksl/ir/SkSLVariableReference.h"

namespace SkSL {

class AutoSymbolTable {
public:
    AutoSymbolTable(IRGenerator* ir)
    : fIR(ir)
    , fPrevious(fIR->fSymbolTable) {
        fIR->pushSymbolTable();
    }

    ~AutoSymbolTable() {
        fIR->popSymbolTable();
        SkASSERT(fPrevious == fIR->fSymbolTable);
    }

    IRGenerator* fIR;
    std::shared_ptr<SymbolTable> fPrevious;
};

IRGenerator::IRGenerator(const Context* context)
        : fContext(*context) {}

void IRGenerator::pushSymbolTable() {
    auto childSymTable = std::make_shared<SymbolTable>(std::move(fSymbolTable), fIsBuiltinCode);
    fSymbolTable = std::move(childSymTable);
}

void IRGenerator::popSymbolTable() {
    fSymbolTable = fSymbolTable->fParent;
}

std::unique_ptr<Extension> IRGenerator::convertExtension(int offset, skstd::string_view name) {
    if (this->programKind() != ProgramKind::kFragment &&
        this->programKind() != ProgramKind::kVertex) {
        this->errorReporter().error(offset, "extensions are not allowed in this kind of program");
        return nullptr;
    }

    return std::make_unique<Extension>(offset, name);
}

std::unique_ptr<Statement> IRGenerator::convertStatement(const ASTNode& statement) {
    switch (statement.fKind) {
        case ASTNode::Kind::kBlock:
            return this->convertBlock(statement);
        case ASTNode::Kind::kVarDeclarations:
            return this->convertVarDeclarationStatement(statement);
        case ASTNode::Kind::kIf:
            return this->convertIf(statement);
        case ASTNode::Kind::kFor:
            return this->convertFor(statement);
        case ASTNode::Kind::kWhile:
            return this->convertWhile(statement);
        case ASTNode::Kind::kDo:
            return this->convertDo(statement);
        case ASTNode::Kind::kSwitch:
            return this->convertSwitch(statement);
        case ASTNode::Kind::kReturn:
            return this->convertReturn(statement);
        case ASTNode::Kind::kBreak:
            return this->convertBreak(statement);
        case ASTNode::Kind::kContinue:
            return this->convertContinue(statement);
        case ASTNode::Kind::kDiscard:
            return this->convertDiscard(statement);
        case ASTNode::Kind::kType:
            // TODO: add IRNode for struct definition inside a function
            return nullptr;
        default:
            // it's an expression
            return this->convertExpressionStatement(statement);
    }
}

std::unique_ptr<Block> IRGenerator::convertBlock(const ASTNode& block) {
    SkASSERT(block.fKind == ASTNode::Kind::kBlock);
    AutoSymbolTable table(this);
    StatementArray statements;
    for (const auto& child : block) {
        std::unique_ptr<Statement> statement = this->convertStatement(child);
        if (statement) {
            statements.push_back(std::move(statement));
        }
    }
    return Block::Make(block.fOffset, std::move(statements), fSymbolTable);
}

std::unique_ptr<Statement> IRGenerator::convertVarDeclarationStatement(const ASTNode& s) {
    SkASSERT(s.fKind == ASTNode::Kind::kVarDeclarations);
    auto decls = this->convertVarDeclarations(s, Variable::Storage::kLocal);
    if (decls.empty()) {
        return nullptr;
    }
    return Block::MakeUnscoped(s.fOffset, std::move(decls));
}

int IRGenerator::convertArraySize(const Type& type, int offset, const ASTNode& s) {
    if (!s) {
        this->errorReporter().error(offset, "array must have a size");
        return 0;
    }
    auto size = this->convertExpression(s);
    if (!size) {
        return 0;
    }
    return type.convertArraySize(fContext, std::move(size));
}

void IRGenerator::checkVarDeclaration(int offset, const Modifiers& modifiers, const Type* baseType,
                                      Variable::Storage storage) {
    if (this->strictES2Mode() && baseType->isArray()) {
        this->errorReporter().error(offset, "array size must appear after variable name");
    }

    if (baseType->componentType().isOpaque() && storage != Variable::Storage::kGlobal) {
        this->errorReporter().error(
                offset,
                "variables of type '" + baseType->displayName() + "' must be global");
    }
    if ((modifiers.fFlags & Modifiers::kIn_Flag) && baseType->isMatrix()) {
        this->errorReporter().error(offset, "'in' variables may not have matrix type");
    }
    if ((modifiers.fFlags & Modifiers::kIn_Flag) && (modifiers.fFlags & Modifiers::kUniform_Flag)) {
        this->errorReporter().error(offset, "'in uniform' variables not permitted");
    }
    if (this->isRuntimeEffect()) {
        if (modifiers.fFlags & Modifiers::kIn_Flag) {
            this->errorReporter().error(offset, "'in' variables not permitted in runtime effects");
        }
    }
    if (baseType->isEffectChild() && !(modifiers.fFlags & Modifiers::kUniform_Flag)) {
        this->errorReporter().error(
                offset, "variables of type '" + baseType->displayName() + "' must be uniform");
    }
    if (modifiers.fLayout.fFlags & Layout::kSRGBUnpremul_Flag) {
        if (!this->isRuntimeEffect()) {
            this->errorReporter().error(offset,
                                        "'srgb_unpremul' is only permitted in runtime effects");
        }
        if (!(modifiers.fFlags & Modifiers::kUniform_Flag)) {
            this->errorReporter().error(offset,
                                        "'srgb_unpremul' is only permitted on 'uniform' variables");
        }
        auto validColorXformType = [](const Type& t) {
            return t.isVector() && t.componentType().isFloat() &&
                   (t.columns() == 3 || t.columns() == 4);
        };
        if (!validColorXformType(*baseType) && !(baseType->isArray() &&
                                                 validColorXformType(baseType->componentType()))) {
            this->errorReporter().error(offset,
                                        "'srgb_unpremul' is only permitted on half3, half4, "
                                        "float3, or float4 variables");
        }
    }
    int permitted = Modifiers::kConst_Flag | Modifiers::kHighp_Flag | Modifiers::kMediump_Flag |
                    Modifiers::kLowp_Flag;
    if (storage == Variable::Storage::kGlobal) {
        permitted |= Modifiers::kIn_Flag | Modifiers::kOut_Flag | Modifiers::kUniform_Flag |
                     Modifiers::kFlat_Flag | Modifiers::kNoPerspective_Flag;
    }
    // TODO(skbug.com/11301): Migrate above checks into building a mask of permitted layout flags
    CheckModifiers(fContext, offset, modifiers, permitted, /*permittedLayoutFlags=*/~0);
}

std::unique_ptr<Variable> IRGenerator::convertVar(int offset, const Modifiers& modifiers,
                                                  const Type* baseType, skstd::string_view name,
                                                  bool isArray,
                                                  std::unique_ptr<Expression> arraySize,
                                                  Variable::Storage storage) {
    if (modifiers.fLayout.fLocation == 0 && modifiers.fLayout.fIndex == 0 &&
        (modifiers.fFlags & Modifiers::kOut_Flag) &&
        this->programKind() == ProgramKind::kFragment && name != Compiler::FRAGCOLOR_NAME) {
        this->errorReporter().error(offset,
                                    "out location=0, index=0 is reserved for sk_FragColor");
    }
    const Type* type = baseType;
    int arraySizeValue = 0;
    if (isArray) {
        SkASSERT(arraySize);
        arraySizeValue = type->convertArraySize(fContext, std::move(arraySize));
        if (!arraySizeValue) {
            return {};
        }
        type = fSymbolTable->addArrayDimension(type, arraySizeValue);
    }
    return std::make_unique<Variable>(offset, this->modifiersPool().add(modifiers), name,
                                      type, fIsBuiltinCode, storage);
}

std::unique_ptr<Statement> IRGenerator::convertVarDeclaration(std::unique_ptr<Variable> var,
                                                              std::unique_ptr<Expression> value,
                                                              bool addToSymbolTable) {
    std::unique_ptr<Statement> varDecl = VarDeclaration::Convert(fContext, var.get(),
                                                                 std::move(value));
    if (!varDecl) {
        return nullptr;
    }

    // Detect the declaration of magical variables.
    if ((var->storage() == Variable::Storage::kGlobal) && var->name() == Compiler::FRAGCOLOR_NAME) {
        // Silently ignore duplicate definitions of `sk_FragColor`.
        const Symbol* symbol = (*fSymbolTable)[var->name()];
        if (symbol) {
            return nullptr;
        }
    } else if ((var->storage() == Variable::Storage::kGlobal ||
                var->storage() == Variable::Storage::kInterfaceBlock) &&
               var->name() == Compiler::RTADJUST_NAME) {
        // `sk_RTAdjust` is special, and makes the IR generator emit position-fixup expressions.
        if (fRTAdjust) {
            this->errorReporter().error(var->fOffset, "duplicate definition of 'sk_RTAdjust'");
            return nullptr;
        }
        if (var->type() != *fContext.fTypes.fFloat4) {
            this->errorReporter().error(var->fOffset, "sk_RTAdjust must have type 'float4'");
            return nullptr;
        }
        fRTAdjust = var.get();
    }

    if (addToSymbolTable) {
        fSymbolTable->add(std::move(var));
    } else {
        fSymbolTable->takeOwnershipOfSymbol(std::move(var));
    }
    return varDecl;
}

std::unique_ptr<Statement> IRGenerator::convertVarDeclaration(int offset,
                                                              const Modifiers& modifiers,
                                                              const Type* baseType,
                                                              skstd::string_view name,
                                                              bool isArray,
                                                              std::unique_ptr<Expression> arraySize,
                                                              std::unique_ptr<Expression> value,
                                                              Variable::Storage storage) {
    std::unique_ptr<Variable> var = this->convertVar(offset, modifiers, baseType, name, isArray,
                                                     std::move(arraySize), storage);
    if (!var) {
        return nullptr;
    }
    return this->convertVarDeclaration(std::move(var), std::move(value));
}

StatementArray IRGenerator::convertVarDeclarations(const ASTNode& decls,
                                                   Variable::Storage storage) {
    SkASSERT(decls.fKind == ASTNode::Kind::kVarDeclarations);
    auto declarationsIter = decls.begin();
    const Modifiers& modifiers = declarationsIter++->getModifiers();
    const ASTNode& rawType = *(declarationsIter++);
    const Type* baseType = this->convertType(rawType);
    if (!baseType) {
        return {};
    }
    baseType = baseType->applyPrecisionQualifiers(fContext, modifiers, fSymbolTable.get(),
                                                  decls.fOffset);
    if (!baseType) {
        return {};
    }

    this->checkVarDeclaration(decls.fOffset, modifiers, baseType, storage);

    StatementArray varDecls;
    for (; declarationsIter != decls.end(); ++declarationsIter) {
        const ASTNode& varDecl = *declarationsIter;
        const ASTNode::VarData& varData = varDecl.getVarData();
        std::unique_ptr<Expression> arraySize;
        std::unique_ptr<Expression> value;
        auto iter = varDecl.begin();
        if (iter != varDecl.end() && varData.fIsArray) {
            if (!*iter) {
                this->errorReporter().error(decls.fOffset, "array must have a size");
                continue;
            }
            arraySize = this->convertExpression(*iter++);
            if (!arraySize) {
                continue;
            }
        }
        if (iter != varDecl.end()) {
            value = this->convertExpression(*iter);
            if (!value) {
                continue;
            }
        }
        std::unique_ptr<Statement> varDeclStmt = this->convertVarDeclaration(varDecl.fOffset,
                                                                             modifiers,
                                                                             baseType,
                                                                             varData.fName,
                                                                             varData.fIsArray,
                                                                             std::move(arraySize),
                                                                             std::move(value),
                                                                             storage);
        if (varDeclStmt) {
            varDecls.push_back(std::move(varDeclStmt));
        }
    }
    return varDecls;
}

std::unique_ptr<ModifiersDeclaration> IRGenerator::convertModifiersDeclaration(const ASTNode& m) {
    if (this->programKind() != ProgramKind::kFragment &&
        this->programKind() != ProgramKind::kVertex) {
        this->errorReporter().error(m.fOffset,
                "layout qualifiers are not allowed in this kind of program");
        return nullptr;
    }

    SkASSERT(m.fKind == ASTNode::Kind::kModifiers);
    Modifiers modifiers = m.getModifiers();
    return std::make_unique<ModifiersDeclaration>(this->modifiersPool().add(modifiers));
}

std::unique_ptr<Statement> IRGenerator::convertIf(const ASTNode& n) {
    SkASSERT(n.fKind == ASTNode::Kind::kIf);
    auto iter = n.begin();
    std::unique_ptr<Expression> test = this->convertExpression(*(iter++));
    if (!test) {
        return nullptr;
    }
    std::unique_ptr<Statement> ifTrue = this->convertStatement(*(iter++));
    if (!ifTrue) {
        return nullptr;
    }
    std::unique_ptr<Statement> ifFalse;
    if (iter != n.end()) {
        ifFalse = this->convertStatement(*(iter++));
        if (!ifFalse) {
            return nullptr;
        }
    }
    bool isStatic = n.getBool();
    return IfStatement::Convert(fContext, n.fOffset, isStatic, std::move(test),
                                std::move(ifTrue), std::move(ifFalse));
}

std::unique_ptr<Statement> IRGenerator::convertFor(const ASTNode& f) {
    SkASSERT(f.fKind == ASTNode::Kind::kFor);
    AutoSymbolTable table(this);
    std::unique_ptr<Statement> initializer;
    auto iter = f.begin();
    if (*iter) {
        initializer = this->convertStatement(*iter);
        if (!initializer) {
            return nullptr;
        }
    }
    ++iter;
    std::unique_ptr<Expression> test;
    if (*iter) {
        test = this->convertExpression(*iter);
        if (!test) {
            return nullptr;
        }
    }
    ++iter;
    std::unique_ptr<Expression> next;
    if (*iter) {
        next = this->convertExpression(*iter);
        if (!next) {
            return nullptr;
        }
    }
    ++iter;
    std::unique_ptr<Statement> statement = this->convertStatement(*iter);
    if (!statement) {
        return nullptr;
    }

    return ForStatement::Convert(fContext, f.fOffset, std::move(initializer), std::move(test),
                                 std::move(next), std::move(statement), fSymbolTable);
}

std::unique_ptr<Statement> IRGenerator::convertWhile(const ASTNode& w) {
    SkASSERT(w.fKind == ASTNode::Kind::kWhile);
    auto iter = w.begin();
    std::unique_ptr<Expression> test = this->convertExpression(*(iter++));
    if (!test) {
        return nullptr;
    }
    std::unique_ptr<Statement> statement = this->convertStatement(*(iter++));
    if (!statement) {
        return nullptr;
    }
    return ForStatement::ConvertWhile(fContext, w.fOffset, std::move(test), std::move(statement),
                                      fSymbolTable);
}

std::unique_ptr<Statement> IRGenerator::convertDo(const ASTNode& d) {
    SkASSERT(d.fKind == ASTNode::Kind::kDo);
    auto iter = d.begin();
    std::unique_ptr<Statement> statement = this->convertStatement(*(iter++));
    if (!statement) {
        return nullptr;
    }
    std::unique_ptr<Expression> test = this->convertExpression(*(iter++));
    if (!test) {
        return nullptr;
    }
    return DoStatement::Convert(fContext, std::move(statement), std::move(test));
}

std::unique_ptr<Statement> IRGenerator::convertSwitch(const ASTNode& s) {
    SkASSERT(s.fKind == ASTNode::Kind::kSwitch);

    auto iter = s.begin();
    std::unique_ptr<Expression> value = this->convertExpression(*(iter++));
    if (!value) {
        return nullptr;
    }
    AutoSymbolTable table(this);
    ExpressionArray caseValues;
    StatementArray caseStatements;
    for (; iter != s.end(); ++iter) {
        const ASTNode& c = *iter;
        SkASSERT(c.fKind == ASTNode::Kind::kSwitchCase);
        std::unique_ptr<Expression>& caseValue = caseValues.emplace_back();
        auto childIter = c.begin();
        if (*childIter) {
            caseValue = this->convertExpression(*childIter);
            if (!caseValue) {
                return nullptr;
            }
        }
        ++childIter;

        StatementArray statements;
        for (; childIter != c.end(); ++childIter) {
            std::unique_ptr<Statement> converted = this->convertStatement(*childIter);
            if (!converted) {
                return nullptr;
            }
            statements.push_back(std::move(converted));
        }

        caseStatements.push_back(Block::MakeUnscoped(c.fOffset, std::move(statements)));
    }
    return SwitchStatement::Convert(fContext, s.fOffset, s.getBool(), std::move(value),
                                    std::move(caseValues), std::move(caseStatements), fSymbolTable);
}

std::unique_ptr<Statement> IRGenerator::convertExpressionStatement(const ASTNode& s) {
    std::unique_ptr<Expression> e = this->convertExpression(s);
    if (!e) {
        return nullptr;
    }
    return ExpressionStatement::Make(fContext, std::move(e));
}

std::unique_ptr<Statement> IRGenerator::convertReturn(int offset,
                                                      std::unique_ptr<Expression> result) {
    return ReturnStatement::Make(offset, std::move(result));
}

std::unique_ptr<Statement> IRGenerator::convertReturn(const ASTNode& r) {
    SkASSERT(r.fKind == ASTNode::Kind::kReturn);
    if (r.begin() != r.end()) {
        if (std::unique_ptr<Expression> value = this->convertExpression(*r.begin())) {
            return this->convertReturn(r.fOffset, std::move(value));
        } else {
            return this->convertReturn(r.fOffset, Poison::Make(r.fOffset, fContext));
        }
    }

    return this->convertReturn(r.fOffset, /*result=*/nullptr);
}

std::unique_ptr<Statement> IRGenerator::convertBreak(const ASTNode& b) {
    SkASSERT(b.fKind == ASTNode::Kind::kBreak);
    return BreakStatement::Make(b.fOffset);
}

std::unique_ptr<Statement> IRGenerator::convertContinue(const ASTNode& c) {
    SkASSERT(c.fKind == ASTNode::Kind::kContinue);
    return ContinueStatement::Make(c.fOffset);
}

std::unique_ptr<Statement> IRGenerator::convertDiscard(const ASTNode& d) {
    SkASSERT(d.fKind == ASTNode::Kind::kDiscard);
    if (this->programKind() != ProgramKind::kFragment) {
        this->errorReporter().error(d.fOffset,
                                    "discard statement is only permitted in fragment shaders");
        return nullptr;
    }
    return DiscardStatement::Make(d.fOffset);
}

void IRGenerator::appendRTAdjustFixupToVertexMain(const FunctionDeclaration& decl, Block* body) {
    using namespace SkSL::dsl;
    using SkSL::dsl::Swizzle;  // disambiguate from SkSL::Swizzle
    using OwnerKind = SkSL::FieldAccess::OwnerKind;

    // If this is a vertex program that uses RTAdjust, and this is main()...
    if ((fRTAdjust || fRTAdjustInterfaceBlock) && decl.isMain() &&
        ProgramKind::kVertex == this->programKind()) {
        // ... append a line to the end of the function body which fixes up sk_Position.
        const Variable* skPerVertex = nullptr;
        if (const ProgramElement* perVertexDecl = fIntrinsics->find(Compiler::PERVERTEX_NAME)) {
            SkASSERT(perVertexDecl->is<SkSL::InterfaceBlock>());
            skPerVertex = &perVertexDecl->as<SkSL::InterfaceBlock>().variable();
        }

        SkASSERT(skPerVertex);
        auto Ref = [](const Variable* var) -> std::unique_ptr<Expression> {
            return VariableReference::Make(/*offset=*/-1, var);
        };
        auto Field = [&](const Variable* var, int idx) -> std::unique_ptr<Expression> {
            return FieldAccess::Make(fContext, Ref(var), idx, OwnerKind::kAnonymousInterfaceBlock);
        };
        auto Pos = [&]() -> DSLExpression {
            return DSLExpression(FieldAccess::Make(fContext, Ref(skPerVertex), /*fieldIndex=*/0,
                                                   OwnerKind::kAnonymousInterfaceBlock));
        };
        auto Adjust = [&]() -> DSLExpression {
            return DSLExpression(fRTAdjustInterfaceBlock
                                         ? Field(fRTAdjustInterfaceBlock, fRTAdjustFieldIndex)
                                         : Ref(fRTAdjust));
        };

        auto fixupStmt = DSLStatement(
            Pos() = Float4(Swizzle(Pos(), X, Y) * Swizzle(Adjust(), X, Z) +
                           Swizzle(Pos(), W, W) * Swizzle(Adjust(), Y, W),
                           0,
                           Pos().w())
        );

        body->children().push_back(fixupStmt.release());
    }
}

void IRGenerator::CheckModifiers(const Context& context,
                                 int offset,
                                 const Modifiers& modifiers,
                                 int permittedModifierFlags,
                                 int permittedLayoutFlags) {
    static constexpr struct { Modifiers::Flag flag; const char* name; } kModifierFlags[] = {
        { Modifiers::kConst_Flag,          "const" },
        { Modifiers::kIn_Flag,             "in" },
        { Modifiers::kOut_Flag,            "out" },
        { Modifiers::kUniform_Flag,        "uniform" },
        { Modifiers::kFlat_Flag,           "flat" },
        { Modifiers::kNoPerspective_Flag,  "noperspective" },
        { Modifiers::kHasSideEffects_Flag, "sk_has_side_effects" },
        { Modifiers::kInline_Flag,         "inline" },
        { Modifiers::kNoInline_Flag,       "noinline" },
        { Modifiers::kHighp_Flag,          "highp" },
        { Modifiers::kMediump_Flag,        "mediump" },
        { Modifiers::kLowp_Flag,           "lowp" },
        { Modifiers::kES3_Flag,            "$es3" },
    };

    int modifierFlags = modifiers.fFlags;
    for (const auto& f : kModifierFlags) {
        if (modifierFlags & f.flag) {
            if (!(permittedModifierFlags & f.flag)) {
                context.fErrors->error(offset, "'" + String(f.name) + "' is not permitted here");
            }
            modifierFlags &= ~f.flag;
        }
    }
    SkASSERT(modifierFlags == 0);

    static constexpr struct { Layout::Flag flag; const char* name; } kLayoutFlags[] = {
        { Layout::kOriginUpperLeft_Flag,          "origin_upper_left"},
        { Layout::kPushConstant_Flag,             "push_constant"},
        { Layout::kBlendSupportAllEquations_Flag, "blend_support_all_equations"},
        { Layout::kSRGBUnpremul_Flag,             "srgb_unpremul"},
        { Layout::kLocation_Flag,                 "location"},
        { Layout::kOffset_Flag,                   "offset"},
        { Layout::kBinding_Flag,                  "binding"},
        { Layout::kIndex_Flag,                    "index"},
        { Layout::kSet_Flag,                      "set"},
        { Layout::kBuiltin_Flag,                  "builtin"},
        { Layout::kInputAttachmentIndex_Flag,     "input_attachment_index"},
    };

    int layoutFlags = modifiers.fLayout.fFlags;
    for (const auto& lf : kLayoutFlags) {
        if (layoutFlags & lf.flag) {
            if (!(permittedLayoutFlags & lf.flag)) {
                context.fErrors->error(
                        offset, "layout qualifier '" + String(lf.name) + "' is not permitted here");
            }
            layoutFlags &= ~lf.flag;
        }
    }
    SkASSERT(layoutFlags == 0);
}

void IRGenerator::convertFunction(const ASTNode& f) {
    auto iter = f.begin();
    const Type* returnType = this->convertType(*(iter++), /*allowVoid=*/true);
    if (returnType == nullptr) {
        return;
    }
    const ASTNode::FunctionData& funcData = f.getFunctionData();
    std::vector<std::unique_ptr<Variable>> parameters;
    parameters.reserve(funcData.fParameterCount);
    for (size_t i = 0; i < funcData.fParameterCount; ++i) {
        const ASTNode& param = *(iter++);
        SkASSERT(param.fKind == ASTNode::Kind::kParameter);
        const ASTNode::ParameterData& pd = param.getParameterData();
        auto paramIter = param.begin();
        const Type* type = this->convertType(*(paramIter++));
        if (!type) {
            return;
        }
        if (pd.fIsArray) {
            int arraySize = this->convertArraySize(*type, param.fOffset, *paramIter++);
            if (!arraySize) {
                return;
            }
            type = fSymbolTable->addArrayDimension(type, arraySize);
        }

        parameters.push_back(std::make_unique<Variable>(param.fOffset,
                                                        this->modifiersPool().add(pd.fModifiers),
                                                        pd.fName,
                                                        type,
                                                        fIsBuiltinCode,
                                                        Variable::Storage::kParameter));
    }

    // Conservatively assume all user-defined functions have side effects.
    Modifiers declModifiers = funcData.fModifiers;
    if (!fIsBuiltinCode) {
        declModifiers.fFlags |= Modifiers::kHasSideEffects_Flag;
    }

    if (fContext.fConfig->fSettings.fForceNoInline) {
        // Apply the `noinline` modifier to every function. This allows us to test Runtime
        // Effects without any inlining, even when the code is later added to a paint.
        declModifiers.fFlags &= ~Modifiers::kInline_Flag;
        declModifiers.fFlags |= Modifiers::kNoInline_Flag;
    }

    const FunctionDeclaration* decl = FunctionDeclaration::Convert(
                                                           fContext,
                                                           *fSymbolTable,
                                                           f.fOffset,
                                                           this->modifiersPool().add(declModifiers),
                                                           funcData.fName,
                                                           std::move(parameters),
                                                           returnType,
                                                           fIsBuiltinCode);
    if (!decl) {
        return;
    }
    if (iter == f.end()) {
        // If there's no body, we've found a prototype.
        fProgramElements->push_back(std::make_unique<FunctionPrototype>(f.fOffset, decl,
                                                                        fIsBuiltinCode));
    } else {
        // Compile function body.
        AutoSymbolTable table(this);
        for (const Variable* param : decl->parameters()) {
            fSymbolTable->addWithoutOwnership(param);
        }
        std::unique_ptr<Block> body = this->convertBlock(*iter);
        if (!body) {
            return;
        }
        this->appendRTAdjustFixupToVertexMain(*decl, body.get());
        std::unique_ptr<FunctionDefinition> result = FunctionDefinition::Convert(
                fContext, f.fOffset, *decl, std::move(body), fIsBuiltinCode);
        decl->setDefinition(result.get());
        result->setSource(&f);
        fProgramElements->push_back(std::move(result));
    }
}

std::unique_ptr<StructDefinition> IRGenerator::convertStructDefinition(const ASTNode& node) {
    SkASSERT(node.fKind == ASTNode::Kind::kType);

    const Type* type = this->convertType(node);
    if (!type) {
        return nullptr;
    }
    if (!type->isStruct()) {
        this->errorReporter().error(node.fOffset,
                                    "expected a struct here, found '" + type->name() + "'");
        return nullptr;
    }
    SkDEBUGCODE(auto [iter, wasInserted] =) fDefinedStructs.insert(type);
    SkASSERT(wasInserted);
    return std::make_unique<StructDefinition>(node.fOffset, *type);
}

void IRGenerator::scanInterfaceBlock(SkSL::InterfaceBlock& intf) {
    const std::vector<Type::Field>& fields = intf.variable().type().componentType().fields();
    for (size_t i = 0; i < fields.size(); ++i) {
        const Type::Field& f = fields[i];
        if (f.fName == Compiler::RTADJUST_NAME) {
            if (*f.fType == *fContext.fTypes.fFloat4) {
                fRTAdjustInterfaceBlock = &intf.variable();
                fRTAdjustFieldIndex = i;
            } else {
                this->errorReporter().error(intf.fOffset, "sk_RTAdjust must have type 'float4'");
            }
        }
    }
}

std::unique_ptr<SkSL::InterfaceBlock> IRGenerator::convertInterfaceBlock(const ASTNode& intf) {
    if (this->programKind() != ProgramKind::kFragment &&
        this->programKind() != ProgramKind::kVertex) {
        this->errorReporter().error(intf.fOffset,
                "interface blocks are not allowed in this kind of program");
        return nullptr;
    }

    SkASSERT(intf.fKind == ASTNode::Kind::kInterfaceBlock);
    const ASTNode::InterfaceBlockData& id = intf.getInterfaceBlockData();
    std::shared_ptr<SymbolTable> old = fSymbolTable;
    std::shared_ptr<SymbolTable> symbols;
    std::vector<Type::Field> fields;
    auto iter = intf.begin();
    {
        AutoSymbolTable table(this);
        symbols = fSymbolTable;
        for (size_t i = 0; i < id.fDeclarationCount; ++i) {
            StatementArray decls = this->convertVarDeclarations(*(iter++),
                                                                Variable::Storage::kInterfaceBlock);
            if (decls.empty()) {
                return nullptr;
            }
            for (const auto& decl : decls) {
                const VarDeclaration& vd = decl->as<VarDeclaration>();
                fields.push_back(Type::Field(vd.var().modifiers(), vd.var().name(),
                                            &vd.var().type()));
            }
        }
    }
    const Type* type = old->takeOwnershipOfSymbol(Type::MakeStructType(intf.fOffset,
                                                                       id.fTypeName,
                                                                       fields));
    int arraySize = 0;
    if (id.fIsArray) {
        const ASTNode& size = *(iter++);
        arraySize = this->convertArraySize(*type, size.fOffset, size);
        if (!arraySize) {
            return nullptr;
        }
        type = symbols->addArrayDimension(type, arraySize);
    }
    const Variable* var = old->takeOwnershipOfSymbol(
            std::make_unique<Variable>(intf.fOffset,
                                       this->modifiersPool().add(id.fModifiers),
                                       id.fInstanceName.length() ? id.fInstanceName : id.fTypeName,
                                       type,
                                       fIsBuiltinCode,
                                       Variable::Storage::kGlobal));
    if (id.fInstanceName.length()) {
        old->addWithoutOwnership(var);
    } else {
        for (size_t i = 0; i < fields.size(); i++) {
            old->add(std::make_unique<Field>(intf.fOffset, var, (int)i));
        }
    }
    std::unique_ptr<SkSL::InterfaceBlock> result = std::make_unique<SkSL::InterfaceBlock>(
            intf.fOffset, var, id.fTypeName, id.fInstanceName, arraySize, symbols);
    this->scanInterfaceBlock(*result);
    return result;
}

void IRGenerator::convertGlobalVarDeclarations(const ASTNode& decl) {
    StatementArray decls = this->convertVarDeclarations(decl, Variable::Storage::kGlobal);
    for (std::unique_ptr<Statement>& stmt : decls) {
        const Type* type = &stmt->as<VarDeclaration>().baseType();
        if (type->isStruct()) {
            auto [iter, wasInserted] = fDefinedStructs.insert(type);
            if (wasInserted) {
                fProgramElements->push_back(
                        std::make_unique<StructDefinition>(decl.fOffset, *type));
            }
        }
        fProgramElements->push_back(std::make_unique<GlobalVarDeclaration>(std::move(stmt)));
    }
}

const Type* IRGenerator::convertType(const ASTNode& type, bool allowVoid) {
    skstd::string_view name = type.getStringView();
    const Symbol* symbol = (*fSymbolTable)[name];
    if (!symbol || !symbol->is<Type>()) {
        this->errorReporter().error(type.fOffset, "unknown type '" + name + "'");
        return nullptr;
    }
    const Type* result = &symbol->as<Type>();
    const bool isArray = (type.begin() != type.end());
    if (result->isVoid() && !allowVoid) {
        this->errorReporter().error(type.fOffset,
                                    "type '" + name + "' not allowed in this context");
        return nullptr;
    }
    if (!fIsBuiltinCode) {
        if (result->containsPrivateFields()) {
            this->errorReporter().error(type.fOffset, "type '" + name + "' is private");
            return nullptr;
        }
        if (this->strictES2Mode() && !result->allowedInES2()) {
            this->errorReporter().error(type.fOffset, "type '" + name + "' is not supported");
            return nullptr;
        }
    }
    if (isArray) {
        auto iter = type.begin();
        int arraySize = this->convertArraySize(*result, type.fOffset, *iter);
        if (!arraySize) {
            return nullptr;
        }
        result = fSymbolTable->addArrayDimension(result, arraySize);
    }
    return result;
}

std::unique_ptr<Expression> IRGenerator::convertExpression(const ASTNode& expr) {
    switch (expr.fKind) {
        case ASTNode::Kind::kBinary:
            return this->convertBinaryExpression(expr);
        case ASTNode::Kind::kBool:
            return BoolLiteral::Make(fContext, expr.fOffset, expr.getBool());
        case ASTNode::Kind::kCall:
            return this->convertCallExpression(expr);
        case ASTNode::Kind::kField:
            return this->convertFieldExpression(expr);
        case ASTNode::Kind::kFloat:
            return FloatLiteral::Make(fContext, expr.fOffset, expr.getFloat());
        case ASTNode::Kind::kIdentifier:
            return this->convertIdentifier(expr);
        case ASTNode::Kind::kIndex:
            return this->convertIndexExpression(expr);
        case ASTNode::Kind::kInt:
            return IntLiteral::Make(fContext, expr.fOffset, expr.getInt());
        case ASTNode::Kind::kPostfix:
            return this->convertPostfixExpression(expr);
        case ASTNode::Kind::kPrefix:
            return this->convertPrefixExpression(expr);
        case ASTNode::Kind::kTernary:
            return this->convertTernaryExpression(expr);
        default:
            SkDEBUGFAIL("unsupported expression");
            return nullptr;
    }
}

std::unique_ptr<Expression> IRGenerator::convertIdentifier(int offset, skstd::string_view name) {
    const Symbol* result = (*fSymbolTable)[name];
    if (!result) {
        this->errorReporter().error(offset, "unknown identifier '" + name + "'");
        return nullptr;
    }
    switch (result->kind()) {
        case Symbol::Kind::kFunctionDeclaration: {
            std::vector<const FunctionDeclaration*> f = {
                &result->as<FunctionDeclaration>()
            };
            return std::make_unique<FunctionReference>(fContext, offset, f);
        }
        case Symbol::Kind::kUnresolvedFunction: {
            const UnresolvedFunction* f = &result->as<UnresolvedFunction>();
            return std::make_unique<FunctionReference>(fContext, offset, f->functions());
        }
        case Symbol::Kind::kVariable: {
            const Variable* var = &result->as<Variable>();
            const Modifiers& modifiers = var->modifiers();
            switch (modifiers.fLayout.fBuiltin) {
                case SK_FRAGCOORD_BUILTIN:
                    if (caps().canUseFragCoord()) {
                        fInputs.fUseFlipRTUniform = true;
                    }
                    break;
                case SK_CLOCKWISE_BUILTIN:
                    fInputs.fUseFlipRTUniform = true;
                    break;
            }
            // default to kRead_RefKind; this will be corrected later if the variable is written to
            return VariableReference::Make(offset, var, VariableReference::RefKind::kRead);
        }
        case Symbol::Kind::kField: {
            const Field* field = &result->as<Field>();
            auto base = VariableReference::Make(offset, &field->owner(),
                                                VariableReference::RefKind::kRead);
            return FieldAccess::Make(fContext, std::move(base), field->fieldIndex(),
                                     FieldAccess::OwnerKind::kAnonymousInterfaceBlock);
        }
        case Symbol::Kind::kType: {
            const Type* t = &result->as<Type>();
            return std::make_unique<TypeReference>(fContext, offset, t);
        }
        case Symbol::Kind::kExternal: {
            const ExternalFunction* r = &result->as<ExternalFunction>();
            return std::make_unique<ExternalFunctionReference>(offset, r);
        }
        default:
            SK_ABORT("unsupported symbol type %d\n", (int) result->kind());
    }
}

std::unique_ptr<Expression> IRGenerator::convertIdentifier(const ASTNode& identifier) {
    return this->convertIdentifier(identifier.fOffset, identifier.getStringView());
}

std::unique_ptr<Expression> IRGenerator::coerce(std::unique_ptr<Expression> expr,
                                                const Type& type) {
    return type.coerceExpression(std::move(expr), fContext);
}

std::unique_ptr<Expression> IRGenerator::convertBinaryExpression(const ASTNode& expression) {
    SkASSERT(expression.fKind == ASTNode::Kind::kBinary);
    auto iter = expression.begin();
    std::unique_ptr<Expression> left = this->convertExpression(*(iter++));
    if (!left) {
        return nullptr;
    }
    std::unique_ptr<Expression> right = this->convertExpression(*(iter++));
    if (!right) {
        return nullptr;
    }
    return BinaryExpression::Convert(fContext, std::move(left), expression.getOperator(),
                                     std::move(right));
}

std::unique_ptr<Expression> IRGenerator::convertTernaryExpression(const ASTNode& node) {
    SkASSERT(node.fKind == ASTNode::Kind::kTernary);
    auto iter = node.begin();
    std::unique_ptr<Expression> test = this->convertExpression(*(iter++));
    if (!test) {
        return nullptr;
    }
    std::unique_ptr<Expression> ifTrue = this->convertExpression(*(iter++));
    if (!ifTrue) {
        return nullptr;
    }
    std::unique_ptr<Expression> ifFalse = this->convertExpression(*(iter++));
    if (!ifFalse) {
        return nullptr;
    }
    return TernaryExpression::Convert(fContext, std::move(test),
                                      std::move(ifTrue), std::move(ifFalse));
}

void IRGenerator::copyIntrinsicIfNeeded(const FunctionDeclaration& function) {
    if (const ProgramElement* found = fIntrinsics->findAndInclude(function.description())) {
        const FunctionDefinition& original = found->as<FunctionDefinition>();

        // Sort the referenced intrinsics into a consistent order; otherwise our output will become
        // non-deterministic.
        std::vector<const FunctionDeclaration*> intrinsics(original.referencedIntrinsics().begin(),
                                                           original.referencedIntrinsics().end());
        std::sort(intrinsics.begin(), intrinsics.end(),
                  [](const FunctionDeclaration* a, const FunctionDeclaration* b) {
                      if (a->isBuiltin() != b->isBuiltin()) {
                          return a->isBuiltin() < b->isBuiltin();
                      }
                      if (a->fOffset != b->fOffset) {
                          return a->fOffset < b->fOffset;
                      }
                      if (a->name() != b->name()) {
                          return a->name() < b->name();
                      }
                      return a->description() < b->description();
                  });
        for (const FunctionDeclaration* f : intrinsics) {
            this->copyIntrinsicIfNeeded(*f);
        }

        fSharedElements->push_back(found);
    }
}

std::unique_ptr<Expression> IRGenerator::call(int offset,
                                              const FunctionDeclaration& function,
                                              ExpressionArray arguments) {
    if (function.isBuiltin()) {
        if (function.intrinsicKind() == k_dFdy_IntrinsicKind) {
            fInputs.fUseFlipRTUniform = true;
        }
        if (!fIsBuiltinCode && fIntrinsics) {
            this->copyIntrinsicIfNeeded(function);
        }
    }

    return FunctionCall::Convert(fContext, offset, function, std::move(arguments));
}

/**
 * Determines the cost of coercing the arguments of a function to the required types. Cost has no
 * particular meaning other than "lower costs are preferred". Returns CoercionCost::Impossible() if
 * the call is not valid.
 */
CoercionCost IRGenerator::callCost(const FunctionDeclaration& function,
                                   const ExpressionArray& arguments) const {
    if (this->strictES2Mode() && (function.modifiers().fFlags & Modifiers::kES3_Flag)) {
        return CoercionCost::Impossible();
    }
    if (function.parameters().size() != arguments.size()) {
        return CoercionCost::Impossible();
    }
    FunctionDeclaration::ParamTypes types;
    const Type* ignored;
    if (!function.determineFinalTypes(arguments, &types, &ignored)) {
        return CoercionCost::Impossible();
    }
    CoercionCost total = CoercionCost::Free();
    for (size_t i = 0; i < arguments.size(); i++) {
        total = total + arguments[i]->coercionCost(*types[i]);
    }
    return total;
}

const FunctionDeclaration* IRGenerator::findBestFunctionForCall(
        const std::vector<const FunctionDeclaration*>& functions,
        const ExpressionArray& arguments) const {
    if (functions.size() == 1) {
        return functions.front();
    }
    CoercionCost bestCost = CoercionCost::Impossible();
    const FunctionDeclaration* best = nullptr;
    for (const auto& f : functions) {
        CoercionCost cost = this->callCost(*f, arguments);
        if (cost < bestCost) {
            bestCost = cost;
            best = f;
        }
    }
    return best;
}

std::unique_ptr<Expression> IRGenerator::call(int offset,
                                              std::unique_ptr<Expression> functionValue,
                                              ExpressionArray arguments) {
    switch (functionValue->kind()) {
        case Expression::Kind::kTypeReference:
            return Constructor::Convert(fContext,
                                        offset,
                                        functionValue->as<TypeReference>().value(),
                                        std::move(arguments));
        case Expression::Kind::kExternalFunctionReference: {
            const ExternalFunction& f = functionValue->as<ExternalFunctionReference>().function();
            int count = f.callParameterCount();
            if (count != (int) arguments.size()) {
                this->errorReporter().error(offset, "external function expected " +
                                                    to_string(count) + " arguments, but found " +
                                                    to_string((int)arguments.size()));
                return nullptr;
            }
            static constexpr int PARAMETER_MAX = 16;
            SkASSERT(count < PARAMETER_MAX);
            const Type* types[PARAMETER_MAX];
            f.getCallParameterTypes(types);
            for (int i = 0; i < count; ++i) {
                arguments[i] = this->coerce(std::move(arguments[i]), *types[i]);
                if (!arguments[i]) {
                    return nullptr;
                }
            }
            return std::make_unique<ExternalFunctionCall>(offset, &f, std::move(arguments));
        }
        case Expression::Kind::kFunctionReference: {
            const FunctionReference& ref = functionValue->as<FunctionReference>();
            const std::vector<const FunctionDeclaration*>& functions = ref.functions();
            const FunctionDeclaration* best = this->findBestFunctionForCall(functions, arguments);
            if (best) {
                return this->call(offset, *best, std::move(arguments));
            }
            String msg = "no match for " + functions[0]->name() + "(";
            String separator;
            for (size_t i = 0; i < arguments.size(); i++) {
                msg += separator;
                separator = ", ";
                msg += arguments[i]->type().displayName();
            }
            msg += ")";
            this->errorReporter().error(offset, msg);
            return nullptr;
        }
        case Expression::Kind::kMethodReference: {
            MethodReference& ref = functionValue->as<MethodReference>();
            arguments.push_back(std::move(ref.self()));

            const std::vector<const FunctionDeclaration*>& functions = ref.functions();
            const FunctionDeclaration* best = this->findBestFunctionForCall(functions, arguments);
            if (best) {
                return this->call(offset, *best, std::move(arguments));
            }
            String msg = "no match for " + arguments.back()->type().displayName() +
                         "::" + functions[0]->name().substr(1) + "(";
            String separator;
            for (size_t i = 0; i < arguments.size() - 1; i++) {
                msg += separator;
                separator = ", ";
                msg += arguments[i]->type().displayName();
            }
            msg += ")";
            this->errorReporter().error(offset, msg);
            return nullptr;
        }
        case Expression::Kind::kPoison:
            return functionValue;
        default:
            this->errorReporter().error(offset, "not a function");
            return nullptr;
    }
}

std::unique_ptr<Expression> IRGenerator::convertPrefixExpression(const ASTNode& expression) {
    SkASSERT(expression.fKind == ASTNode::Kind::kPrefix);
    std::unique_ptr<Expression> base = this->convertExpression(*expression.begin());
    if (!base) {
        return nullptr;
    }
    return PrefixExpression::Convert(fContext, expression.getOperator(), std::move(base));
}

std::unique_ptr<Expression> IRGenerator::convertSwizzle(std::unique_ptr<Expression> base,
                                                        skstd::string_view fields) {
    return Swizzle::Convert(fContext, std::move(base), fields);
}

std::unique_ptr<Expression> IRGenerator::convertIndexExpression(const ASTNode& index) {
    SkASSERT(index.fKind == ASTNode::Kind::kIndex);
    auto iter = index.begin();
    std::unique_ptr<Expression> base = this->convertExpression(*(iter++));
    if (!base) {
        return nullptr;
    }
    if (iter == index.end()) {
        if (base->is<TypeReference>()) {
            this->errorReporter().error(index.fOffset, "array must have a size");
        } else {
            this->errorReporter().error(base->fOffset, "missing index in '[]'");
        }
        return nullptr;
    }
    std::unique_ptr<Expression> converted = this->convertExpression(*(iter++));
    if (!converted) {
        return nullptr;
    }
    return IndexExpression::Convert(fContext, *fSymbolTable, std::move(base), std::move(converted));
}

std::unique_ptr<Expression> IRGenerator::convertCallExpression(const ASTNode& callNode) {
    SkASSERT(callNode.fKind == ASTNode::Kind::kCall);
    auto iter = callNode.begin();
    std::unique_ptr<Expression> base = this->convertExpression(*(iter++));
    if (!base) {
        return nullptr;
    }
    ExpressionArray arguments;
    for (; iter != callNode.end(); ++iter) {
        std::unique_ptr<Expression> converted = this->convertExpression(*iter);
        if (!converted) {
            return nullptr;
        }
        arguments.push_back(std::move(converted));
    }
    return this->call(callNode.fOffset, std::move(base), std::move(arguments));
}

std::unique_ptr<Expression> IRGenerator::convertFieldExpression(const ASTNode& fieldNode) {
    std::unique_ptr<Expression> base = this->convertExpression(*fieldNode.begin());
    if (!base) {
        return nullptr;
    }
    const skstd::string_view& field = fieldNode.getStringView();
    const Type& baseType = base->type();
    if (baseType == *fContext.fTypes.fSkCaps || baseType.isStruct() || baseType.isEffectChild()) {
        return FieldAccess::Convert(fContext, *fSymbolTable, std::move(base), field);
    }
    return this->convertSwizzle(std::move(base), field);
}

std::unique_ptr<Expression> IRGenerator::convertPostfixExpression(const ASTNode& expression) {
    SkASSERT(expression.fKind == ASTNode::Kind::kPostfix);
    std::unique_ptr<Expression> base = this->convertExpression(*expression.begin());
    if (!base) {
        return nullptr;
    }
    return PostfixExpression::Convert(fContext, std::move(base), expression.getOperator());
}

void IRGenerator::findAndDeclareBuiltinVariables() {
    class BuiltinVariableScanner : public ProgramVisitor {
    public:
        BuiltinVariableScanner(IRGenerator* generator) : fGenerator(generator) {}

        void addDeclaringElement(const String& name) {
            // If this is the *first* time we've seen this builtin, findAndInclude will return
            // the corresponding ProgramElement.
            if (const ProgramElement* decl = fGenerator->fIntrinsics->findAndInclude(name)) {
                SkASSERT(decl->is<GlobalVarDeclaration>() || decl->is<SkSL::InterfaceBlock>());
                fNewElements.push_back(decl);
            }
        }

        bool visitProgramElement(const ProgramElement& pe) override {
            if (pe.is<FunctionDefinition>()) {
                const FunctionDefinition& funcDef = pe.as<FunctionDefinition>();
                // We synthesize writes to sk_FragColor if main() returns a color, even if it's
                // otherwise unreferenced. Check main's return type to see if it's half4.
                if (funcDef.declaration().isMain() &&
                    funcDef.declaration().returnType() == *fGenerator->fContext.fTypes.fHalf4) {
                    fPreserveFragColor = true;
                }
            }
            return INHERITED::visitProgramElement(pe);
        }

        bool visitExpression(const Expression& e) override {
            if (e.is<VariableReference>() && e.as<VariableReference>().variable()->isBuiltin()) {
                this->addDeclaringElement(String(e.as<VariableReference>().variable()->name()));
            }
            return INHERITED::visitExpression(e);
        }

        IRGenerator* fGenerator;
        std::vector<const ProgramElement*> fNewElements;
        bool fPreserveFragColor = false;

        using INHERITED = ProgramVisitor;
        using INHERITED::visitProgramElement;
    };

    BuiltinVariableScanner scanner(this);
    SkASSERT(fProgramElements);
    for (auto& e : *fProgramElements) {
        scanner.visitProgramElement(*e);
    }

    if (scanner.fPreserveFragColor) {
        // main() returns a half4, so make sure we don't dead-strip sk_FragColor.
        scanner.addDeclaringElement(Compiler::FRAGCOLOR_NAME);
    }

    switch (this->programKind()) {
        case ProgramKind::kFragment:
            // Vulkan requires certain builtin variables be present, even if they're unused. At one
            // time, validation errors would result if sk_Clockwise was missing. Now, it's just
            // (Adreno) driver bugs that drop or corrupt draws if they're missing.
            scanner.addDeclaringElement("sk_Clockwise");
            break;
        default:
            break;
    }

    fSharedElements->insert(
            fSharedElements->begin(), scanner.fNewElements.begin(), scanner.fNewElements.end());
}

void IRGenerator::start(const ParsedModule& base,
                        bool isBuiltinCode,
                        std::vector<std::unique_ptr<ProgramElement>>* elements,
                        std::vector<const ProgramElement*>* sharedElements) {
    fProgramElements = elements;
    fSharedElements = sharedElements;
    fSymbolTable = base.fSymbols;
    fIntrinsics = base.fIntrinsics.get();
    if (fIntrinsics) {
        fIntrinsics->resetAlreadyIncluded();
    }
    fIsBuiltinCode = isBuiltinCode;

    fInputs = {};
    fRTAdjust = nullptr;
    fRTAdjustInterfaceBlock = nullptr;
    fDefinedStructs.clear();
    this->pushSymbolTable();

    if (this->settings().fExternalFunctions) {
        // Add any external values to the new symbol table, so they're only visible to this Program.
        for (const std::unique_ptr<ExternalFunction>& ef : *this->settings().fExternalFunctions) {
            fSymbolTable->addWithoutOwnership(ef.get());
        }
    }

    if (this->isRuntimeEffect() && !fContext.fConfig->fSettings.fEnforceES2Restrictions) {
        // We're compiling a runtime effect, but we're not enforcing ES2 restrictions. Add various
        // non-ES2 types to our symbol table to allow them to be tested.
        fSymbolTable->addAlias("mat2x2", fContext.fTypes.fFloat2x2.get());
        fSymbolTable->addAlias("mat2x3", fContext.fTypes.fFloat2x3.get());
        fSymbolTable->addAlias("mat2x4", fContext.fTypes.fFloat2x4.get());
        fSymbolTable->addAlias("mat3x2", fContext.fTypes.fFloat3x2.get());
        fSymbolTable->addAlias("mat3x3", fContext.fTypes.fFloat3x3.get());
        fSymbolTable->addAlias("mat3x4", fContext.fTypes.fFloat3x4.get());
        fSymbolTable->addAlias("mat4x2", fContext.fTypes.fFloat4x2.get());
        fSymbolTable->addAlias("mat4x3", fContext.fTypes.fFloat4x3.get());
        fSymbolTable->addAlias("mat4x4", fContext.fTypes.fFloat4x4.get());

        fSymbolTable->addAlias("float2x3", fContext.fTypes.fFloat2x3.get());
        fSymbolTable->addAlias("float2x4", fContext.fTypes.fFloat2x4.get());
        fSymbolTable->addAlias("float3x2", fContext.fTypes.fFloat3x2.get());
        fSymbolTable->addAlias("float3x4", fContext.fTypes.fFloat3x4.get());
        fSymbolTable->addAlias("float4x2", fContext.fTypes.fFloat4x2.get());
        fSymbolTable->addAlias("float4x3", fContext.fTypes.fFloat4x3.get());

        fSymbolTable->addAlias("half2x3", fContext.fTypes.fHalf2x3.get());
        fSymbolTable->addAlias("half2x4", fContext.fTypes.fHalf2x4.get());
        fSymbolTable->addAlias("half3x2", fContext.fTypes.fHalf3x2.get());
        fSymbolTable->addAlias("half3x4", fContext.fTypes.fHalf3x4.get());
        fSymbolTable->addAlias("half4x2", fContext.fTypes.fHalf4x2.get());
        fSymbolTable->addAlias("half4x3", fContext.fTypes.fHalf4x3.get());

        fSymbolTable->addAlias("uint", fContext.fTypes.fUInt.get());
        fSymbolTable->addAlias("uint2", fContext.fTypes.fUInt2.get());
        fSymbolTable->addAlias("uint3", fContext.fTypes.fUInt3.get());
        fSymbolTable->addAlias("uint4", fContext.fTypes.fUInt4.get());

        fSymbolTable->addAlias("short", fContext.fTypes.fShort.get());
        fSymbolTable->addAlias("short2", fContext.fTypes.fShort2.get());
        fSymbolTable->addAlias("short3", fContext.fTypes.fShort3.get());
        fSymbolTable->addAlias("short4", fContext.fTypes.fShort4.get());

        fSymbolTable->addAlias("ushort", fContext.fTypes.fUShort.get());
        fSymbolTable->addAlias("ushort2", fContext.fTypes.fUShort2.get());
        fSymbolTable->addAlias("ushort3", fContext.fTypes.fUShort3.get());
        fSymbolTable->addAlias("ushort4", fContext.fTypes.fUShort4.get());
    }
}

IRGenerator::IRBundle IRGenerator::finish() {
    // Variables defined in the pre-includes need their declaring elements added to the program
    if (!fIsBuiltinCode && fIntrinsics) {
        this->findAndDeclareBuiltinVariables();
    }

    return IRBundle{std::move(*fProgramElements),
                    std::move(*fSharedElements),
                    std::move(fSymbolTable),
                    fInputs};
}

IRGenerator::IRBundle IRGenerator::convertProgram(
        const ParsedModule& base,
        bool isBuiltinCode,
        skstd::string_view text) {
    Parser parser(text, *fSymbolTable, this->errorReporter());
    fFile = parser.compilationUnit();
    if (this->errorReporter().errorCount() == 0) {
        SkASSERT(fFile);
        for (const auto& decl : fFile->root()) {
            switch (decl.fKind) {
                case ASTNode::Kind::kVarDeclarations:
                    this->convertGlobalVarDeclarations(decl);
                    break;

                case ASTNode::Kind::kFunction:
                    this->convertFunction(decl);
                    break;

                case ASTNode::Kind::kModifiers: {
                    std::unique_ptr<ModifiersDeclaration> f =
                                                            this->convertModifiersDeclaration(decl);
                    if (f) {
                        fProgramElements->push_back(std::move(f));
                    }
                    break;
                }
                case ASTNode::Kind::kInterfaceBlock: {
                    std::unique_ptr<SkSL::InterfaceBlock> i = this->convertInterfaceBlock(decl);
                    if (i) {
                        fProgramElements->push_back(std::move(i));
                    }
                    break;
                }
                case ASTNode::Kind::kExtension: {
                    std::unique_ptr<Extension> e = this->convertExtension(decl.fOffset,
                                                                          decl.getStringView());
                    if (e) {
                        fProgramElements->push_back(std::move(e));
                    }
                    break;
                }
                case ASTNode::Kind::kType: {
                    std::unique_ptr<StructDefinition> s = this->convertStructDefinition(decl);
                    if (s) {
                        fProgramElements->push_back(std::move(s));
                    }
                    break;
                }
                default:
                    SkDEBUGFAIL("unsupported declaration");
                    break;
            }
        }
    }
    return this->finish();
}

}  // namespace SkSL
