/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLCompiler.h"

#include <memory>
#include <unordered_set>

#include "src/core/SkScopeExit.h"
#include "src/core/SkTraceEvent.h"
#include "src/sksl/SkSLAnalysis.h"
#include "src/sksl/SkSLCFGGenerator.h"
#include "src/sksl/SkSLCPPCodeGenerator.h"
#include "src/sksl/SkSLGLSLCodeGenerator.h"
#include "src/sksl/SkSLHCodeGenerator.h"
#include "src/sksl/SkSLIRGenerator.h"
#include "src/sksl/SkSLMetalCodeGenerator.h"
#include "src/sksl/SkSLOperators.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/SkSLRehydrator.h"
#include "src/sksl/SkSLSPIRVCodeGenerator.h"
#include "src/sksl/SkSLSPIRVtoHLSL.h"
#include "src/sksl/ir/SkSLEnum.h"
#include "src/sksl/ir/SkSLExpression.h"
#include "src/sksl/ir/SkSLExpressionStatement.h"
#include "src/sksl/ir/SkSLFunctionCall.h"
#include "src/sksl/ir/SkSLIntLiteral.h"
#include "src/sksl/ir/SkSLModifiersDeclaration.h"
#include "src/sksl/ir/SkSLNop.h"
#include "src/sksl/ir/SkSLSymbolTable.h"
#include "src/sksl/ir/SkSLTernaryExpression.h"
#include "src/sksl/ir/SkSLUnresolvedFunction.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"
#include "src/utils/SkBitSet.h"

#include <fstream>

#if !defined(SKSL_STANDALONE) & SK_SUPPORT_GPU
#include "include/gpu/GrContextOptions.h"
#include "src/gpu/GrShaderCaps.h"
#endif

#ifdef SK_ENABLE_SPIRV_VALIDATION
#include "spirv-tools/libspirv.hpp"
#endif

#if defined(SKSL_STANDALONE)

// In standalone mode, we load the textual sksl source files. GN generates or copies these files
// to the skslc executable directory. The "data" in this mode is just the filename.
#define MODULE_DATA(name) MakeModulePath("sksl_" #name ".sksl")

#else

// At runtime, we load the dehydrated sksl data files. The data is a (pointer, size) pair.
#include "src/sksl/generated/sksl_fp.dehydrated.sksl"
#include "src/sksl/generated/sksl_frag.dehydrated.sksl"
#include "src/sksl/generated/sksl_geom.dehydrated.sksl"
#include "src/sksl/generated/sksl_gpu.dehydrated.sksl"
#include "src/sksl/generated/sksl_public.dehydrated.sksl"
#include "src/sksl/generated/sksl_runtime.dehydrated.sksl"
#include "src/sksl/generated/sksl_vert.dehydrated.sksl"

#define MODULE_DATA(name) MakeModuleData(SKSL_INCLUDE_sksl_##name,\
                                         SKSL_INCLUDE_sksl_##name##_LENGTH)

#endif

namespace SkSL {

using RefKind = VariableReference::RefKind;

class AutoSource {
public:
    AutoSource(Compiler* compiler, const String* source)
            : fCompiler(compiler), fOldSource(fCompiler->fSource) {
        fCompiler->fSource = source;
    }

    ~AutoSource() { fCompiler->fSource = fOldSource; }

    Compiler* fCompiler;
    const String* fOldSource;
};

Compiler::Compiler(const ShaderCapsClass* caps)
        : fContext(std::make_shared<Context>(/*errors=*/*this, *caps))
        , fInliner(fContext.get())
        , fErrorCount(0) {
    SkASSERT(caps);
    fRootSymbolTable = std::make_shared<SymbolTable>(this, /*builtin=*/true);
    fPrivateSymbolTable = std::make_shared<SymbolTable>(fRootSymbolTable, /*builtin=*/true);
    fIRGenerator = std::make_unique<IRGenerator>(fContext.get());

#define TYPE(t) fContext->fTypes.f ## t .get()

    const SkSL::Symbol* rootTypes[] = {
        TYPE(Void),

        TYPE( Float), TYPE( Float2), TYPE( Float3), TYPE( Float4),
        TYPE(  Half), TYPE(  Half2), TYPE(  Half3), TYPE(  Half4),
        TYPE(   Int), TYPE(   Int2), TYPE(   Int3), TYPE(   Int4),
        TYPE(  Bool), TYPE(  Bool2), TYPE(  Bool3), TYPE(  Bool4),

        TYPE(Float2x2), TYPE(Float3x3), TYPE(Float4x4),
        TYPE( Half2x2), TYPE( Half3x3), TYPE(Half4x4),

        TYPE(SquareMat), TYPE(SquareHMat),

        TYPE(GenType), TYPE(GenHType), TYPE(GenIType), TYPE(GenBType),
        TYPE(Vec),     TYPE(HVec),     TYPE(IVec),     TYPE(BVec),

        TYPE(FragmentProcessor),
    };

    const SkSL::Symbol* privateTypes[] = {
        TYPE(  UInt), TYPE(  UInt2), TYPE(  UInt3), TYPE(  UInt4),
        TYPE( Short), TYPE( Short2), TYPE( Short3), TYPE( Short4),
        TYPE(UShort), TYPE(UShort2), TYPE(UShort3), TYPE(UShort4),
        TYPE(  Byte), TYPE(  Byte2), TYPE(  Byte3), TYPE(  Byte4),
        TYPE( UByte), TYPE( UByte2), TYPE( UByte3), TYPE( UByte4),

        TYPE(GenUType), TYPE(UVec),
        TYPE(SVec), TYPE(USVec), TYPE(ByteVec), TYPE(UByteVec),

        TYPE(Float2x3), TYPE(Float2x4),
        TYPE(Float3x2), TYPE(Float3x4),
        TYPE(Float4x2), TYPE(Float4x3),

        TYPE(Half2x3),  TYPE(Half2x4),
        TYPE(Half3x2),  TYPE(Half3x4),
        TYPE(Half4x2),  TYPE(Half4x3),

        TYPE(Mat), TYPE(HMat),

        TYPE(Sampler1D), TYPE(Sampler2D), TYPE(Sampler3D),
        TYPE(SamplerExternalOES),
        TYPE(Sampler2DRect),

        TYPE(ISampler2D),
        TYPE(SubpassInput), TYPE(SubpassInputMS),

        TYPE(Sampler),
        TYPE(Texture2D),
    };

    for (const SkSL::Symbol* type : rootTypes) {
        fRootSymbolTable->addWithoutOwnership(type);
    }
    for (const SkSL::Symbol* type : privateTypes) {
        fPrivateSymbolTable->addWithoutOwnership(type);
    }

#undef TYPE

    // sk_Caps is "builtin", but all references to it are resolved to Settings, so we don't need to
    // treat it as builtin (ie, no need to clone it into the Program).
    fPrivateSymbolTable->add(
            std::make_unique<Variable>(/*offset=*/-1,
                                       fIRGenerator->fModifiers->addToPool(Modifiers()),
                                       "sk_Caps",
                                       fContext->fTypes.fSkCaps.get(),
                                       /*builtin=*/false,
                                       Variable::Storage::kGlobal));

    fRootModule = {fRootSymbolTable, /*fIntrinsics=*/nullptr};
    fPrivateModule = {fPrivateSymbolTable, /*fIntrinsics=*/nullptr};
}

Compiler::~Compiler() {}

const ParsedModule& Compiler::loadGPUModule() {
    if (!fGPUModule.fSymbols) {
        fGPUModule = this->parseModule(ProgramKind::kFragment, MODULE_DATA(gpu), fPrivateModule);
    }
    return fGPUModule;
}

const ParsedModule& Compiler::loadFragmentModule() {
    if (!fFragmentModule.fSymbols) {
        fFragmentModule = this->parseModule(ProgramKind::kFragment, MODULE_DATA(frag),
                                            this->loadGPUModule());
    }
    return fFragmentModule;
}

const ParsedModule& Compiler::loadVertexModule() {
    if (!fVertexModule.fSymbols) {
        fVertexModule = this->parseModule(ProgramKind::kVertex, MODULE_DATA(vert),
                                          this->loadGPUModule());
    }
    return fVertexModule;
}

const ParsedModule& Compiler::loadGeometryModule() {
    if (!fGeometryModule.fSymbols) {
        fGeometryModule = this->parseModule(ProgramKind::kGeometry, MODULE_DATA(geom),
                                            this->loadGPUModule());
    }
    return fGeometryModule;
}

const ParsedModule& Compiler::loadFPModule() {
    if (!fFPModule.fSymbols) {
        fFPModule = this->parseModule(ProgramKind::kFragmentProcessor, MODULE_DATA(fp),
                                      this->loadGPUModule());
    }
    return fFPModule;
}

const ParsedModule& Compiler::loadPublicModule() {
    if (!fPublicModule.fSymbols) {
        fPublicModule = this->parseModule(ProgramKind::kGeneric, MODULE_DATA(public), fRootModule);
    }
    return fPublicModule;
}

const ParsedModule& Compiler::loadRuntimeEffectModule() {
    if (!fRuntimeEffectModule.fSymbols) {
        fRuntimeEffectModule = this->parseModule(ProgramKind::kRuntimeEffect, MODULE_DATA(runtime),
                                                 this->loadPublicModule());

        // Add some aliases to the runtime effect module so that it's friendlier, and more like GLSL
        fRuntimeEffectModule.fSymbols->addAlias("shader", fContext->fTypes.fFragmentProcessor.get());

        fRuntimeEffectModule.fSymbols->addAlias("vec2", fContext->fTypes.fFloat2.get());
        fRuntimeEffectModule.fSymbols->addAlias("vec3", fContext->fTypes.fFloat3.get());
        fRuntimeEffectModule.fSymbols->addAlias("vec4", fContext->fTypes.fFloat4.get());

        fRuntimeEffectModule.fSymbols->addAlias("bvec2", fContext->fTypes.fBool2.get());
        fRuntimeEffectModule.fSymbols->addAlias("bvec3", fContext->fTypes.fBool3.get());
        fRuntimeEffectModule.fSymbols->addAlias("bvec4", fContext->fTypes.fBool4.get());

        fRuntimeEffectModule.fSymbols->addAlias("mat2", fContext->fTypes.fFloat2x2.get());
        fRuntimeEffectModule.fSymbols->addAlias("mat3", fContext->fTypes.fFloat3x3.get());
        fRuntimeEffectModule.fSymbols->addAlias("mat4", fContext->fTypes.fFloat4x4.get());
    }
    return fRuntimeEffectModule;
}

const ParsedModule& Compiler::moduleForProgramKind(ProgramKind kind) {
    switch (kind) {
        case ProgramKind::kVertex:            return this->loadVertexModule();        break;
        case ProgramKind::kFragment:          return this->loadFragmentModule();      break;
        case ProgramKind::kGeometry:          return this->loadGeometryModule();      break;
        case ProgramKind::kFragmentProcessor: return this->loadFPModule();            break;
        case ProgramKind::kRuntimeEffect:     return this->loadRuntimeEffectModule(); break;
        case ProgramKind::kGeneric:           return this->loadPublicModule();        break;
    }
    SkUNREACHABLE;
}

LoadedModule Compiler::loadModule(ProgramKind kind,
                                  ModuleData data,
                                  std::shared_ptr<SymbolTable> base) {
    if (!base) {
        // NOTE: This is a workaround. The only time 'base' is null is when dehydrating includes.
        // In that case, skslc doesn't know which module it's preparing, nor what the correct base
        // module is. We can't use 'Root', because many GPU intrinsics reference private types,
        // like samplers or textures. Today, 'Private' does contain the union of all known types,
        // so this is safe. If we ever have types that only exist in 'Public' (for example), this
        // logic needs to be smarter (by choosing the correct base for the module we're compiling).
        base = fPrivateSymbolTable;
    }

#if defined(SKSL_STANDALONE)
    SkASSERT(data.fPath);
    std::ifstream in(data.fPath);
    std::unique_ptr<String> text = std::make_unique<String>(std::istreambuf_iterator<char>(in),
                                                            std::istreambuf_iterator<char>());
    if (in.rdstate()) {
        printf("error reading %s\n", data.fPath);
        abort();
    }
    const String* source = fRootSymbolTable->takeOwnershipOfString(std::move(text));
    AutoSource as(this, source);

    SkASSERT(fIRGenerator->fCanInline);
    fIRGenerator->fCanInline = false;

    ProgramConfig config;
    config.fKind = kind;
    config.fSettings.fReplaceSettings = false;

    fContext->fConfig = &config;
    SK_AT_SCOPE_EXIT(fContext->fConfig = nullptr);

    ParsedModule baseModule = {base, /*fIntrinsics=*/nullptr};
    IRGenerator::IRBundle ir = fIRGenerator->convertProgram(baseModule, /*isBuiltinCode=*/true,
                                                            source->c_str(), source->length(),
                                                            /*externalFunctions=*/nullptr);
    SkASSERT(ir.fSharedElements.empty());
    LoadedModule module = { kind, std::move(ir.fSymbolTable), std::move(ir.fElements) };
    fIRGenerator->fCanInline = true;
    if (this->fErrorCount) {
        printf("Unexpected errors: %s\n", this->fErrorText.c_str());
        SkDEBUGFAILF("%s %s\n", data.fPath, this->fErrorText.c_str());
    }
    fModifiers.push_back(std::move(ir.fModifiers));
#else
    SkASSERT(data.fData && (data.fSize != 0));
    Rehydrator rehydrator(fContext.get(), fIRGenerator->fModifiers.get(), base, this,
                          data.fData, data.fSize);
    LoadedModule module = { kind, rehydrator.symbolTable(), rehydrator.elements() };
    fModifiers.push_back(fIRGenerator->releaseModifiers());
#endif

    return module;
}

ParsedModule Compiler::parseModule(ProgramKind kind, ModuleData data, const ParsedModule& base) {
    LoadedModule module = this->loadModule(kind, data, base.fSymbols);
    this->optimize(module);

    // For modules that just declare (but don't define) intrinsic functions, there will be no new
    // program elements. In that case, we can share our parent's intrinsic map:
    if (module.fElements.empty()) {
        return {module.fSymbols, base.fIntrinsics};
    }

    auto intrinsics = std::make_shared<IRIntrinsicMap>(base.fIntrinsics.get());

    // Now, transfer all of the program elements to an intrinsic map. This maps certain types of
    // global objects to the declaring ProgramElement.
    for (std::unique_ptr<ProgramElement>& element : module.fElements) {
        switch (element->kind()) {
            case ProgramElement::Kind::kFunction: {
                const FunctionDefinition& f = element->as<FunctionDefinition>();
                SkASSERT(f.declaration().isBuiltin());
                intrinsics->insertOrDie(f.declaration().description(), std::move(element));
                break;
            }
            case ProgramElement::Kind::kFunctionPrototype: {
                // These are already in the symbol table.
                break;
            }
            case ProgramElement::Kind::kEnum: {
                const Enum& e = element->as<Enum>();
                SkASSERT(e.isBuiltin());
                intrinsics->insertOrDie(e.typeName(), std::move(element));
                break;
            }
            case ProgramElement::Kind::kGlobalVar: {
                const GlobalVarDeclaration& global = element->as<GlobalVarDeclaration>();
                const Variable& var = global.declaration()->as<VarDeclaration>().var();
                SkASSERT(var.isBuiltin());
                intrinsics->insertOrDie(var.name(), std::move(element));
                break;
            }
            case ProgramElement::Kind::kInterfaceBlock: {
                const Variable& var = element->as<InterfaceBlock>().variable();
                SkASSERT(var.isBuiltin());
                intrinsics->insertOrDie(var.name(), std::move(element));
                break;
            }
            default:
                printf("Unsupported element: %s\n", element->description().c_str());
                SkASSERT(false);
                break;
        }
    }

    return {module.fSymbols, std::move(intrinsics)};
}

void Compiler::scanCFG(CFG* cfg, BlockId blockId, SkBitSet* processedSet) {
    BasicBlock& block = cfg->fBlocks[blockId];

    // compute definitions after this block
    DefinitionMap after = block.fBefore;
    for (const BasicBlock::Node& n : block.fNodes) {
        after.addDefinitions(*fContext, n);
    }

    // propagate definitions to exits
    for (BlockId exitId : block.fExits) {
        if (exitId == blockId) {
            continue;
        }
        BasicBlock& exit = cfg->fBlocks[exitId];
        for (const auto& [var, e1] : after) {
            std::unique_ptr<Expression>** exitDef = exit.fBefore.find(var);
            if (!exitDef) {
                // exit has no definition for it, just copy it and reprocess exit block
                processedSet->reset(exitId);
                exit.fBefore.set(var, e1);
            } else {
                // exit has a (possibly different) value already defined
                std::unique_ptr<Expression>* e2 = *exitDef;
                if (e1 != e2) {
                    // definition has changed, merge and reprocess the exit block
                    processedSet->reset(exitId);
                    if (e1 && e2) {
                        *exitDef = (std::unique_ptr<Expression>*)&fContext->fDefined_Expression;
                    } else {
                        *exitDef = nullptr;
                    }
                }
            }
        }
    }
}

/**
 * Returns true if assigning to this lvalue has no effect.
 */
static bool is_dead(const Expression& lvalue, ProgramUsage* usage) {
    switch (lvalue.kind()) {
        case Expression::Kind::kVariableReference:
            return usage->isDead(*lvalue.as<VariableReference>().variable());
        case Expression::Kind::kSwizzle:
            return is_dead(*lvalue.as<Swizzle>().base(), usage);
        case Expression::Kind::kFieldAccess:
            return is_dead(*lvalue.as<FieldAccess>().base(), usage);
        case Expression::Kind::kIndex: {
            const IndexExpression& idx = lvalue.as<IndexExpression>();
            return is_dead(*idx.base(), usage) &&
                   !idx.index()->hasProperty(Expression::Property::kSideEffects);
        }
        case Expression::Kind::kTernary: {
            const TernaryExpression& t = lvalue.as<TernaryExpression>();
            return !t.test()->hasSideEffects() &&
                   is_dead(*t.ifTrue(), usage) &&
                   is_dead(*t.ifFalse(), usage);
        }
        default:
            SkDEBUGFAILF("invalid lvalue: %s\n", lvalue.description().c_str());
            return false;
    }
}

/**
 * Returns true if this is an assignment which can be collapsed down to just the right hand side due
 * to a dead target and lack of side effects on the left hand side.
 */
static bool dead_assignment(const BinaryExpression& b, ProgramUsage* usage) {
    if (!b.getOperator().isAssignment()) {
        return false;
    }
    return is_dead(*b.left(), usage);
}

/**
 * Returns true if both expression trees are the same. The left side is expected to be an lvalue.
 * This only needs to check for trees that can plausibly terminate in a variable, so some basic
 * candidates like `FloatLiteral` are missing.
 */
static bool is_matching_expression_tree(const Expression& left, const Expression& right) {
    if (left.kind() != right.kind() || left.type() != right.type()) {
        return false;
    }

    switch (left.kind()) {
        case Expression::Kind::kIntLiteral:
            return left.as<IntLiteral>().value() == right.as<IntLiteral>().value();

        case Expression::Kind::kFieldAccess:
            return left.as<FieldAccess>().fieldIndex() == right.as<FieldAccess>().fieldIndex() &&
                   is_matching_expression_tree(*left.as<FieldAccess>().base(),
                                               *right.as<FieldAccess>().base());

        case Expression::Kind::kIndex:
            return is_matching_expression_tree(*left.as<IndexExpression>().index(),
                                               *right.as<IndexExpression>().index()) &&
                   is_matching_expression_tree(*left.as<IndexExpression>().base(),
                                               *right.as<IndexExpression>().base());

        case Expression::Kind::kSwizzle:
            return left.as<Swizzle>().components() == right.as<Swizzle>().components() &&
                   is_matching_expression_tree(*left.as<Swizzle>().base(),
                                               *right.as<Swizzle>().base());

        case Expression::Kind::kVariableReference:
            return left.as<VariableReference>().variable() ==
                   right.as<VariableReference>().variable();

        default:
            return false;
    }
}

static bool self_assignment(const BinaryExpression& b) {
    return b.getOperator().kind() == Token::Kind::TK_EQ &&
           is_matching_expression_tree(*b.left(), *b.right());
}

void Compiler::computeDataFlow(CFG* cfg) {
    cfg->fBlocks[cfg->fStart].fBefore.computeStartState(*cfg);

    // We set bits in the "processed" set after a block has been scanned.
    SkBitSet processedSet(cfg->fBlocks.size());
    while (SkBitSet::OptionalIndex blockId = processedSet.findFirstUnset()) {
        processedSet.set(*blockId);
        this->scanCFG(cfg, *blockId, &processedSet);
    }
}

/**
 * Attempts to replace the expression pointed to by iter with a new one (in both the CFG and the
 * IR). If the expression can be cleanly removed, returns true and updates the iterator to point to
 * the newly-inserted element. Otherwise updates only the IR and returns false (and the CFG will
 * need to be regenerated).
 */
static bool try_replace_expression(BasicBlock* b,
                                   std::vector<BasicBlock::Node>::iterator* iter,
                                   std::unique_ptr<Expression>* newExpression) {
    std::unique_ptr<Expression>* target = (*iter)->expression();
    if (!b->tryRemoveExpression(iter)) {
        *target = std::move(*newExpression);
        return false;
    }
    *target = std::move(*newExpression);
    return b->tryInsertExpression(iter, target);
}

/**
 * Returns true if the expression is a constant numeric literal with the specified value, or a
 * constant vector with all elements equal to the specified value.
 */
template <typename T = SKSL_FLOAT>
static bool is_constant(const Expression& expr, T value) {
    switch (expr.kind()) {
        case Expression::Kind::kIntLiteral:
            return expr.as<IntLiteral>().value() == value;

        case Expression::Kind::kFloatLiteral:
            return expr.as<FloatLiteral>().value() == value;

        case Expression::Kind::kConstructor: {
            const Constructor& constructor = expr.as<Constructor>();
            if (constructor.isCompileTimeConstant()) {
                const Type& constructorType = constructor.type();
                switch (constructorType.typeKind()) {
                    case Type::TypeKind::kVector:
                        if (constructor.componentType().isFloat()) {
                            for (int i = 0; i < constructorType.columns(); ++i) {
                                if (constructor.getFVecComponent(i) != value) {
                                    return false;
                                }
                            }
                            return true;
                        } else if (constructor.componentType().isInteger()) {
                            for (int i = 0; i < constructorType.columns(); ++i) {
                                if (constructor.getIVecComponent(i) != value) {
                                    return false;
                                }
                            }
                            return true;
                        }
                        // Other types (e.g. boolean) might occur, but aren't supported here.
                        return false;

                    case Type::TypeKind::kScalar:
                        SkASSERT(constructor.arguments().size() == 1);
                        return is_constant<T>(*constructor.arguments()[0], value);

                    default:
                        return false;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

/**
 * Collapses the binary expression pointed to by iter down to just the right side (in both the IR
 * and CFG structures).
 */
static void delete_left(BasicBlock* b,
                        std::vector<BasicBlock::Node>::iterator* iter,
                        Compiler::OptimizationContext* optimizationContext) {
    optimizationContext->fUpdated = true;
    std::unique_ptr<Expression>* target = (*iter)->expression();
    BinaryExpression& bin = (*target)->as<BinaryExpression>();
    Expression& left = *bin.left();
    std::unique_ptr<Expression>& rightPointer = bin.right();
    SkASSERT(!left.hasSideEffects());
    bool result;
    if (bin.getOperator().kind() == Token::Kind::TK_EQ) {
        result = b->tryRemoveLValueBefore(iter, &left);
    } else {
        result = b->tryRemoveExpressionBefore(iter, &left);
    }
    // Remove references within LHS.
    optimizationContext->fUsage->remove(&left);
    *target = std::move(rightPointer);
    if (!result) {
        optimizationContext->fNeedsRescan = true;
        return;
    }
    if (*iter == b->fNodes.begin()) {
        optimizationContext->fNeedsRescan = true;
        return;
    }
    --(*iter);
    if (!(*iter)->isExpression() || (*iter)->expression() != &rightPointer) {
        optimizationContext->fNeedsRescan = true;
        return;
    }
    *iter = b->fNodes.erase(*iter);
    SkASSERT((*iter)->expression() == target);
}

/**
 * Collapses the binary expression pointed to by iter down to just the left side (in both the IR and
 * CFG structures).
 */
static void delete_right(BasicBlock* b,
                         std::vector<BasicBlock::Node>::iterator* iter,
                         Compiler::OptimizationContext* optimizationContext) {
    optimizationContext->fUpdated = true;
    std::unique_ptr<Expression>* target = (*iter)->expression();
    BinaryExpression& bin = (*target)->as<BinaryExpression>();
    std::unique_ptr<Expression>& leftPointer = bin.left();
    Expression& right = *bin.right();
    SkASSERT(!right.hasSideEffects());
    // Remove references within RHS.
    optimizationContext->fUsage->remove(&right);
    if (!b->tryRemoveExpressionBefore(iter, &right)) {
        *target = std::move(leftPointer);
        optimizationContext->fNeedsRescan = true;
        return;
    }
    *target = std::move(leftPointer);
    if (*iter == b->fNodes.begin()) {
        optimizationContext->fNeedsRescan = true;
        return;
    }
    --(*iter);
    if ((!(*iter)->isExpression() || (*iter)->expression() != &leftPointer)) {
        optimizationContext->fNeedsRescan = true;
        return;
    }
    *iter = b->fNodes.erase(*iter);
    SkASSERT((*iter)->expression() == target);
}

/**
 * Constructs the specified type using a single argument.
 */
static std::unique_ptr<Expression> construct(const Type* type, std::unique_ptr<Expression> v) {
    ExpressionArray args;
    args.push_back(std::move(v));
    return std::make_unique<Constructor>(/*offset=*/-1, *type, std::move(args));
}

/**
 * Used in the implementations of vectorize_left and vectorize_right. Given a vector type and an
 * expression x, deletes the expression pointed to by iter and replaces it with <type>(x).
 */
static void vectorize(BasicBlock* b,
                      std::vector<BasicBlock::Node>::iterator* iter,
                      const Type& type,
                      std::unique_ptr<Expression>* otherExpression,
                      Compiler::OptimizationContext* optimizationContext) {
    SkASSERT((*(*iter)->expression())->kind() == Expression::Kind::kBinary);
    SkASSERT(type.isVector());
    SkASSERT((*otherExpression)->type().isScalar());
    optimizationContext->fUpdated = true;
    std::unique_ptr<Expression>* target = (*iter)->expression();
    if (!b->tryRemoveExpression(iter)) {
        *target = construct(&type, std::move(*otherExpression));
        optimizationContext->fNeedsRescan = true;
    } else {
        *target = construct(&type, std::move(*otherExpression));
        if (!b->tryInsertExpression(iter, target)) {
            optimizationContext->fNeedsRescan = true;
        }
    }
}

/**
 * Given a binary expression of the form x <op> vec<n>(y), deletes the right side and vectorizes the
 * left to yield vec<n>(x).
 */
static void vectorize_left(BasicBlock* b,
                           std::vector<BasicBlock::Node>::iterator* iter,
                           Compiler::OptimizationContext* optimizationContext) {
    BinaryExpression& bin = (*(*iter)->expression())->as<BinaryExpression>();
    // Remove references within RHS. Vectorization of LHS doesn't change reference counts.
    optimizationContext->fUsage->remove(bin.right().get());
    vectorize(b, iter, bin.right()->type(), &bin.left(), optimizationContext);
}

/**
 * Given a binary expression of the form vec<n>(x) <op> y, deletes the left side and vectorizes the
 * right to yield vec<n>(y).
 */
static void vectorize_right(BasicBlock* b,
                            std::vector<BasicBlock::Node>::iterator* iter,
                            Compiler::OptimizationContext* optimizationContext) {
    BinaryExpression& bin = (*(*iter)->expression())->as<BinaryExpression>();
    // Remove references within LHS. Vectorization of RHS doesn't change reference counts.
    optimizationContext->fUsage->remove(bin.left().get());
    vectorize(b, iter, bin.left()->type(), &bin.right(), optimizationContext);
}

void Compiler::simplifyExpression(DefinitionMap& definitions,
                                  BasicBlock& b,
                                  std::vector<BasicBlock::Node>::iterator* iter,
                                  OptimizationContext* optimizationContext) {
    Expression* expr = (*iter)->expression()->get();
    SkASSERT(expr);

    if ((*iter)->fConstantPropagation) {
        std::unique_ptr<Expression> optimized = expr->constantPropagate(*fIRGenerator,
                                                                        definitions);
        if (optimized) {
            optimizationContext->fUpdated = true;
            optimized = fIRGenerator->coerce(std::move(optimized), expr->type());
            SkASSERT(optimized);
            // Remove references within 'expr', add references within 'optimized'
            optimizationContext->fUsage->replace(expr, optimized.get());
            if (!try_replace_expression(&b, iter, &optimized)) {
                optimizationContext->fNeedsRescan = true;
                return;
            }
            SkASSERT((*iter)->isExpression());
            expr = (*iter)->expression()->get();
        }
    }
    switch (expr->kind()) {
        case Expression::Kind::kVariableReference: {
            const VariableReference& ref = expr->as<VariableReference>();
            const Variable* var = ref.variable();
            if (fContext->fConfig->fSettings.fDeadCodeElimination &&
                ref.refKind() != VariableReference::RefKind::kWrite &&
                ref.refKind() != VariableReference::RefKind::kPointer &&
                var->storage() == Variable::Storage::kLocal && !definitions.get(var) &&
                optimizationContext->fSilences.find(var) == optimizationContext->fSilences.end()) {
                optimizationContext->fSilences.insert(var);
                this->error(expr->fOffset,
                            "'" + var->name() + "' has not been assigned");
            }
            break;
        }
        case Expression::Kind::kTernary: {
            TernaryExpression* t = &expr->as<TernaryExpression>();
            if (t->test()->is<BoolLiteral>()) {
                // ternary has a constant test, replace it with either the true or
                // false branch
                if (t->test()->as<BoolLiteral>().value()) {
                    (*iter)->setExpression(std::move(t->ifTrue()), optimizationContext->fUsage);
                } else {
                    (*iter)->setExpression(std::move(t->ifFalse()), optimizationContext->fUsage);
                }
                optimizationContext->fUpdated = true;
                optimizationContext->fNeedsRescan = true;
            }
            break;
        }
        case Expression::Kind::kBinary: {
            BinaryExpression* bin = &expr->as<BinaryExpression>();
            if (dead_assignment(*bin, optimizationContext->fUsage) || self_assignment(*bin)) {
                delete_left(&b, iter, optimizationContext);
                break;
            }
            Expression& left = *bin->left();
            Expression& right = *bin->right();
            const Type& leftType = left.type();
            const Type& rightType = right.type();
            // collapse useless expressions like x * 1 or x + 0
            if ((!leftType.isScalar() && !leftType.isVector()) ||
                (!rightType.isScalar() && !rightType.isVector())) {
                break;
            }
            switch (bin->getOperator().kind()) {
                case Token::Kind::TK_STAR:
                    if (is_constant(left, 1)) {
                        if (leftType.isVector() && rightType.isScalar()) {
                            // float4(1) * x -> float4(x)
                            vectorize_right(&b, iter, optimizationContext);
                        } else {
                            // 1 * x -> x
                            // 1 * float4(x) -> float4(x)
                            // float4(1) * float4(x) -> float4(x)
                            delete_left(&b, iter, optimizationContext);
                        }
                    }
                    else if (is_constant(left, 0)) {
                        if (leftType.isScalar() && rightType.isVector() &&
                            !right.hasSideEffects()) {
                            // 0 * float4(x) -> float4(0)
                            vectorize_left(&b, iter, optimizationContext);
                        } else {
                            // 0 * x -> 0
                            // float4(0) * x -> float4(0)
                            // float4(0) * float4(x) -> float4(0)
                            if (!right.hasSideEffects()) {
                                delete_right(&b, iter, optimizationContext);
                            }
                        }
                    }
                    else if (is_constant(right, 1)) {
                        if (leftType.isScalar() && rightType.isVector()) {
                            // x * float4(1) -> float4(x)
                            vectorize_left(&b, iter, optimizationContext);
                        } else {
                            // x * 1 -> x
                            // float4(x) * 1 -> float4(x)
                            // float4(x) * float4(1) -> float4(x)
                            delete_right(&b, iter, optimizationContext);
                        }
                    }
                    else if (is_constant(right, 0)) {
                        if (leftType.isVector() && rightType.isScalar() && !left.hasSideEffects()) {
                            // float4(x) * 0 -> float4(0)
                            vectorize_right(&b, iter, optimizationContext);
                        } else {
                            // x * 0 -> 0
                            // x * float4(0) -> float4(0)
                            // float4(x) * float4(0) -> float4(0)
                            if (!left.hasSideEffects()) {
                                delete_left(&b, iter, optimizationContext);
                            }
                        }
                    }
                    break;
                case Token::Kind::TK_PLUS:
                    if (is_constant(left, 0)) {
                        if (leftType.isVector() && rightType.isScalar()) {
                            // float4(0) + x -> float4(x)
                            vectorize_right(&b, iter, optimizationContext);
                        } else {
                            // 0 + x -> x
                            // 0 + float4(x) -> float4(x)
                            // float4(0) + float4(x) -> float4(x)
                            delete_left(&b, iter, optimizationContext);
                        }
                    } else if (is_constant(right, 0)) {
                        if (leftType.isScalar() && rightType.isVector()) {
                            // x + float4(0) -> float4(x)
                            vectorize_left(&b, iter, optimizationContext);
                        } else {
                            // x + 0 -> x
                            // float4(x) + 0 -> float4(x)
                            // float4(x) + float4(0) -> float4(x)
                            delete_right(&b, iter, optimizationContext);
                        }
                    }
                    break;
                case Token::Kind::TK_MINUS:
                    if (is_constant(right, 0)) {
                        if (leftType.isScalar() && rightType.isVector()) {
                            // x - float4(0) -> float4(x)
                            vectorize_left(&b, iter, optimizationContext);
                        } else {
                            // x - 0 -> x
                            // float4(x) - 0 -> float4(x)
                            // float4(x) - float4(0) -> float4(x)
                            delete_right(&b, iter, optimizationContext);
                        }
                    }
                    break;
                case Token::Kind::TK_SLASH:
                    if (is_constant(right, 1)) {
                        if (leftType.isScalar() && rightType.isVector()) {
                            // x / float4(1) -> float4(x)
                            vectorize_left(&b, iter, optimizationContext);
                        } else {
                            // x / 1 -> x
                            // float4(x) / 1 -> float4(x)
                            // float4(x) / float4(1) -> float4(x)
                            delete_right(&b, iter, optimizationContext);
                        }
                    } else if (is_constant(left, 0)) {
                        if (leftType.isScalar() && rightType.isVector() &&
                            !right.hasSideEffects()) {
                            // 0 / float4(x) -> float4(0)
                            vectorize_left(&b, iter, optimizationContext);
                        } else {
                            // 0 / x -> 0
                            // float4(0) / x -> float4(0)
                            // float4(0) / float4(x) -> float4(0)
                            if (!right.hasSideEffects()) {
                                delete_right(&b, iter, optimizationContext);
                            }
                        }
                    }
                    break;
                case Token::Kind::TK_PLUSEQ:
                    if (is_constant(right, 0)) {
                        Analysis::UpdateRefKind(&left, RefKind::kRead);
                        delete_right(&b, iter, optimizationContext);
                    }
                    break;
                case Token::Kind::TK_MINUSEQ:
                    if (is_constant(right, 0)) {
                        Analysis::UpdateRefKind(&left, RefKind::kRead);
                        delete_right(&b, iter, optimizationContext);
                    }
                    break;
                case Token::Kind::TK_STAREQ:
                    if (is_constant(right, 1)) {
                        Analysis::UpdateRefKind(&left, RefKind::kRead);
                        delete_right(&b, iter, optimizationContext);
                    }
                    break;
                case Token::Kind::TK_SLASHEQ:
                    if (is_constant(right, 1)) {
                        Analysis::UpdateRefKind(&left, RefKind::kRead);
                        delete_right(&b, iter, optimizationContext);
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        case Expression::Kind::kConstructor: {
            // Find constructors embedded inside constructors and flatten them out where possible.
            //   -  float4(float2(1, 2), 3, 4)                -->  float4(1, 2, 3, 4)
            //   -  float4(w, float3(sin(x), cos(y), tan(z))) -->  float4(w, sin(x), cos(y), tan(z))
            // Leave single-argument constructors alone, though. These might be casts or splats.
            Constructor& c = expr->as<Constructor>();
            if (c.type().columns() > 1) {
                // Inspect each constructor argument to see if it's a candidate for flattening.
                // Remember matched arguments in a bitfield, "argsToOptimize".
                int argsToOptimize = 0;
                int currBit = 1;
                for (const std::unique_ptr<Expression>& arg : c.arguments()) {
                    if (arg->is<Constructor>()) {
                        Constructor& inner = arg->as<Constructor>();
                        if (inner.arguments().size() > 1 &&
                            inner.type().componentType() == c.type().componentType()) {
                            argsToOptimize |= currBit;
                        }
                    }
                    currBit <<= 1;
                }
                if (argsToOptimize) {
                    // We found at least one argument that could be flattened out. Re-walk the
                    // constructor args and flatten the candidates we found during our initial pass.
                    ExpressionArray flattened;
                    flattened.reserve_back(c.type().columns());
                    currBit = 1;
                    for (const std::unique_ptr<Expression>& arg : c.arguments()) {
                        if (argsToOptimize & currBit) {
                            Constructor& inner = arg->as<Constructor>();
                            for (const std::unique_ptr<Expression>& innerArg : inner.arguments()) {
                                flattened.push_back(innerArg->clone());
                            }
                        } else {
                            flattened.push_back(arg->clone());
                        }
                        currBit <<= 1;
                    }
                    std::unique_ptr<Expression> replacement = std::make_unique<Constructor>(
                            c.fOffset, c.type(), std::move(flattened));
                    // We're replacing an expression with a cloned version; we'll need a rescan.
                    // No fUsage change: `float2(float(x), y)` and `float2(x, y)` have equivalent
                    // reference counts.
                    try_replace_expression(&b, iter, &replacement);
                    optimizationContext->fUpdated = true;
                    optimizationContext->fNeedsRescan = true;
                    break;
                }
            }
            break;
        }
        case Expression::Kind::kSwizzle: {
            Swizzle& s = expr->as<Swizzle>();
            // Detect identity swizzles like `foo.rgba`.
            if ((int) s.components().size() == s.base()->type().columns()) {
                bool identity = true;
                for (int i = 0; i < (int) s.components().size(); ++i) {
                    if (s.components()[i] != i) {
                        identity = false;
                        break;
                    }
                }
                if (identity) {
                    optimizationContext->fUpdated = true;
                    // No fUsage change: foo.rgba and foo have equivalent reference counts
                    if (!try_replace_expression(&b, iter, &s.base())) {
                        optimizationContext->fNeedsRescan = true;
                        return;
                    }
                    SkASSERT((*iter)->isExpression());
                    break;
                }
            }
            // Detect swizzles of swizzles, e.g. replace `foo.argb.r000` with `foo.a000`.
            if (s.base()->is<Swizzle>()) {
                Swizzle& base = s.base()->as<Swizzle>();
                ComponentArray final;
                for (int c : s.components()) {
                    final.push_back(base.components()[c]);
                }
                std::unique_ptr<Expression> replacement(new Swizzle(*fContext, base.base()->clone(),
                                                                    final));
                // We're replacing an expression with a cloned version; we'll need a rescan.
                // No fUsage change: `foo.gbr.gbr` and `foo.brg` have equivalent reference counts
                try_replace_expression(&b, iter, &replacement);
                optimizationContext->fUpdated = true;
                optimizationContext->fNeedsRescan = true;
                break;
            }
            // Optimize swizzles of constructors.
            if (s.base()->is<Constructor>()) {
                Constructor& base = s.base()->as<Constructor>();
                std::unique_ptr<Expression> replacement;
                const Type& componentType = base.type().componentType();
                int swizzleSize = s.components().size();

                // The IR generator has already converted any zero/one swizzle components into
                // constructors containing zero/one args. Confirm that this is true by checking that
                // our swizzle components are all `xyzw` (values 0 through 3).
                SkASSERT(std::all_of(s.components().begin(), s.components().end(),
                                     [](int8_t c) { return c >= 0 && c <= 3; }));

                if (base.arguments().size() == 1 && base.arguments().front()->type().isScalar()) {
                    // `half4(scalar).zyy` can be optimized to `half3(scalar)`. The swizzle
                    // components don't actually matter since all fields are the same.
                    const Expression& argument = *base.arguments().front();
                    const Type& constructorType = componentType.toCompound(*fContext, swizzleSize,
                                                                           /*rows=*/1);
                    replacement = Constructor::SimplifyConversion(constructorType, argument);
                    if (!replacement) {
                        ExpressionArray newArgs;
                        newArgs.push_back(argument.clone());
                        replacement = std::make_unique<Constructor>(base.fOffset, constructorType,
                                                                    std::move(newArgs));
                    }

                    // We're replacing an expression with a cloned version; we'll need a rescan.
                    // There's no fUsage change: `half4(foo).xy` and `half2(foo)` have equivalent
                    // reference counts.
                    try_replace_expression(&b, iter, &replacement);
                    optimizationContext->fUpdated = true;
                    optimizationContext->fNeedsRescan = true;
                    break;
                }

                // Swizzles can duplicate some elements and discard others, e.g.
                // `half4(1, 2, 3, 4).xxz` --> `half3(1, 1, 3)`. However, there are constraints:
                // - Expressions with side effects need to occur exactly once, even if they
                //   would otherwise be swizzle-eliminated
                // - Non-trivial expressions should not be repeated, but elimination is OK.
                //
                // Look up the argument for the constructor at each index. This is typically simple
                // but for weird cases like `half4(bar.yz, half2(foo))`, it can be harder than it
                // seems. This example would result in:
                //     argMap[0] = {.fArgIndex = 0, .fComponent = 0}   (bar.yz     .x)
                //     argMap[1] = {.fArgIndex = 0, .fComponent = 1}   (bar.yz     .y)
                //     argMap[2] = {.fArgIndex = 1, .fComponent = 0}   (half2(foo) .x)
                //     argMap[3] = {.fArgIndex = 1, .fComponent = 1}   (half2(foo) .y)
                struct ConstructorArgMap {
                    int8_t fArgIndex;
                    int8_t fComponent;
                };

                int numConstructorArgs = base.type().columns();
                ConstructorArgMap argMap[4] = {};
                int writeIdx = 0;
                for (int argIdx = 0; argIdx < (int) base.arguments().size(); ++argIdx) {
                    const Expression& expr = *base.arguments()[argIdx];
                    int argWidth = expr.type().columns();
                    for (int componentIdx = 0; componentIdx < argWidth; ++componentIdx) {
                        argMap[writeIdx].fArgIndex = argIdx;
                        argMap[writeIdx].fComponent = componentIdx;
                        ++writeIdx;
                    }
                }
                SkASSERT(writeIdx == numConstructorArgs);

                // Count up the number of times each constructor argument is used by the
                // swizzle.
                //    `half4(bar.yz, half2(foo)).xwxy` -> { 3, 1 }
                // - bar.yz    is referenced 3 times, by `.x_xy`
                // - half(foo) is referenced 1 time,  by `._w__`
                int8_t exprUsed[4] = {};
                for (int c : s.components()) {
                    exprUsed[argMap[c].fArgIndex]++;
                }

                bool safeToOptimize = true;
                for (int index = 0; index < numConstructorArgs; ++index) {
                    int8_t constructorArgIndex = argMap[index].fArgIndex;
                    const Expression& baseArg = *base.arguments()[constructorArgIndex];

                    // Check that non-trivial expressions are not swizzled in more than once.
                    if (exprUsed[constructorArgIndex] > 1 &&
                            !Analysis::IsTrivialExpression(baseArg)) {
                        safeToOptimize = false;
                        break;
                    }
                    // Check that side-effect-bearing expressions are swizzled in exactly once.
                    if (exprUsed[constructorArgIndex] != 1 && baseArg.hasSideEffects()) {
                        safeToOptimize = false;
                        break;
                    }
                }

                if (safeToOptimize) {
                    struct ReorderedArgument {
                        int8_t fArgIndex;
                        ComponentArray fComponents;
                    };
                    SkSTArray<4, ReorderedArgument> reorderedArgs;
                    for (int c : s.components()) {
                        const ConstructorArgMap& argument = argMap[c];
                        const Expression& baseArg = *base.arguments()[argument.fArgIndex];

                        if (baseArg.type().isScalar()) {
                            // This argument is a scalar; add it to the list as-is.
                            SkASSERT(argument.fComponent == 0);
                            reorderedArgs.push_back({argument.fArgIndex,
                                                     ComponentArray{}});
                        } else {
                            // This argument is a component from a vector.
                            SkASSERT(argument.fComponent < baseArg.type().columns());
                            if (reorderedArgs.empty() ||
                                reorderedArgs.back().fArgIndex != argument.fArgIndex) {
                                // This can't be combined with the previous argument. Add a new one.
                                reorderedArgs.push_back({argument.fArgIndex,
                                                         ComponentArray{argument.fComponent}});
                            } else {
                                // Since we know this argument uses components, it should already
                                // have at least one component set.
                                SkASSERT(!reorderedArgs.back().fComponents.empty());
                                // Build up the current argument with one more component.
                                reorderedArgs.back().fComponents.push_back(argument.fComponent);
                            }
                        }
                    }

                    // Convert our reordered argument list to an actual array of expressions, with
                    // the new order and any new inner swizzles that need to be applied. Note that
                    // we expect followup passes to clean up the inner swizzles.
                    ExpressionArray newArgs;
                    newArgs.reserve_back(swizzleSize);
                    for (const ReorderedArgument& reorderedArg : reorderedArgs) {
                        const Expression& baseArg = *base.arguments()[reorderedArg.fArgIndex];
                        if (reorderedArg.fComponents.empty()) {
                            newArgs.push_back(baseArg.clone());
                        } else {
                            newArgs.push_back(std::make_unique<Swizzle>(*fContext, baseArg.clone(),
                                                                        reorderedArg.fComponents));
                        }
                    }

                    // Create a new constructor.
                    replacement = std::make_unique<Constructor>(
                            base.fOffset,
                            componentType.toCompound(*fContext, swizzleSize, /*rows=*/1),
                            std::move(newArgs));

                    // Remove references within 'expr', add references within 'replacement.'
                    optimizationContext->fUsage->replace(expr, replacement.get());

                    // We're replacing an expression with a cloned version; we'll need a rescan.
                    try_replace_expression(&b, iter, &replacement);
                    optimizationContext->fUpdated = true;
                    optimizationContext->fNeedsRescan = true;
                }
                break;
            }
            break;
        }
        default:
            break;
    }
}

void Compiler::simplifyStatement(DefinitionMap& definitions,
                                 BasicBlock& b,
                                 std::vector<BasicBlock::Node>::iterator* iter,
                                 OptimizationContext* optimizationContext) {
    ProgramUsage* usage = optimizationContext->fUsage;
    Statement* stmt = (*iter)->statement()->get();
    switch (stmt->kind()) {
        case Statement::Kind::kVarDeclaration: {
            const auto& varDecl = stmt->as<VarDeclaration>();
            if (usage->isDead(varDecl.var()) &&
                (!varDecl.value() ||
                 !varDecl.value()->hasSideEffects())) {
                if (varDecl.value()) {
                    SkASSERT((*iter)->statement()->get() == stmt);
                    if (!b.tryRemoveExpressionBefore(iter, varDecl.value().get())) {
                        optimizationContext->fNeedsRescan = true;
                    }
                }
                // There can still be (soon to be removed) references to the variable at this point.
                // Allowing the VarDeclaration to be destroyed here will break those variable's
                // initialValue()s, so we hang on to them until optimization is finished.
                std::unique_ptr<Statement> old = (*iter)->setStatement(std::make_unique<Nop>(),
                                                                       usage);
                optimizationContext->fOwnedStatements.push_back(std::move(old));
                optimizationContext->fUpdated = true;
            }
            break;
        }
        case Statement::Kind::kIf: {
            IfStatement& i = stmt->as<IfStatement>();
            if (i.test()->kind() == Expression::Kind::kBoolLiteral) {
                // constant if, collapse down to a single branch
                if (i.test()->as<BoolLiteral>().value()) {
                    SkASSERT(i.ifTrue());
                    (*iter)->setStatement(std::move(i.ifTrue()), usage);
                } else {
                    if (i.ifFalse()) {
                        (*iter)->setStatement(std::move(i.ifFalse()), usage);
                    } else {
                        (*iter)->setStatement(std::make_unique<Nop>(), usage);
                    }
                }
                optimizationContext->fUpdated = true;
                optimizationContext->fNeedsRescan = true;
                break;
            }
            if (i.ifFalse() && i.ifFalse()->isEmpty()) {
                // else block doesn't do anything, remove it
                i.ifFalse().reset();
                optimizationContext->fUpdated = true;
                optimizationContext->fNeedsRescan = true;
            }
            if (!i.ifFalse() && i.ifTrue()->isEmpty()) {
                // if block doesn't do anything, no else block
                if (i.test()->hasSideEffects()) {
                    // test has side effects, keep it
                    (*iter)->setStatement(
                            std::make_unique<ExpressionStatement>(std::move(i.test())), usage);
                } else {
                    // no if, no else, no test side effects, kill the whole if
                    // statement
                    (*iter)->setStatement(std::make_unique<Nop>(), usage);
                }
                optimizationContext->fUpdated = true;
                optimizationContext->fNeedsRescan = true;
            }
            break;
        }
        case Statement::Kind::kSwitch: {
            // TODO(skia:11319): this optimization logic is redundant with the static-switch
            // optimization code found in SwitchStatement.cpp.
            SwitchStatement& s = stmt->as<SwitchStatement>();
            int64_t switchValue;
            if (ConstantFolder::GetConstantInt(*s.value(), &switchValue)) {
                // switch is constant, replace it with the case that matches
                bool found = false;
                SwitchCase* defaultCase = nullptr;
                for (const std::unique_ptr<SwitchCase>& c : s.cases()) {
                    if (!c->value()) {
                        defaultCase = c.get();
                        continue;
                    }
                    int64_t caseValue;
                    SkAssertResult(ConstantFolder::GetConstantInt(*c->value(), &caseValue));
                    if (caseValue == switchValue) {
                        std::unique_ptr<Statement> newBlock =
                                SwitchStatement::BlockForCase(&s.cases(), c.get(), s.symbols());
                        if (newBlock) {
                            (*iter)->setStatement(std::move(newBlock), usage);
                            found = true;
                            break;
                        } else {
                            if (s.isStatic() &&
                                !fContext->fConfig->fSettings.fPermitInvalidStaticTests) {
                                auto [iter, didInsert] = optimizationContext->fSilences.insert(&s);
                                if (didInsert) {
                                    this->error(s.fOffset, "static switch contains non-static "
                                                           "conditional exit");
                                }
                            }
                            return; // can't simplify
                        }
                    }
                }
                if (!found) {
                    // no matching case. use default if it exists, or kill the whole thing
                    if (defaultCase) {
                        std::unique_ptr<Statement> newBlock =
                                SwitchStatement::BlockForCase(&s.cases(), defaultCase, s.symbols());
                        if (newBlock) {
                            (*iter)->setStatement(std::move(newBlock), usage);
                        } else {
                            if (s.isStatic() &&
                                !fContext->fConfig->fSettings.fPermitInvalidStaticTests) {
                                auto [iter, didInsert] = optimizationContext->fSilences.insert(&s);
                                if (didInsert) {
                                    this->error(s.fOffset, "static switch contains non-static "
                                                           "conditional exit");
                                }
                            }
                            return; // can't simplify
                        }
                    } else {
                        (*iter)->setStatement(std::make_unique<Nop>(), usage);
                    }
                }
                optimizationContext->fUpdated = true;
                optimizationContext->fNeedsRescan = true;
            }
            break;
        }
        case Statement::Kind::kExpression: {
            ExpressionStatement& e = stmt->as<ExpressionStatement>();
            SkASSERT((*iter)->statement()->get() == &e);
            if (!e.expression()->hasSideEffects()) {
                // Expression statement with no side effects, kill it
                if (!b.tryRemoveExpressionBefore(iter, e.expression().get())) {
                    optimizationContext->fNeedsRescan = true;
                }
                SkASSERT((*iter)->statement()->get() == stmt);
                (*iter)->setStatement(std::make_unique<Nop>(), usage);
                optimizationContext->fUpdated = true;
            }
            break;
        }
        default:
            break;
    }
}

bool Compiler::scanCFG(FunctionDefinition& f, ProgramUsage* usage) {
    bool madeChanges = false;

    CFG cfg = CFGGenerator().getCFG(f);
    this->computeDataFlow(&cfg);

    if (fContext->fConfig->fSettings.fDeadCodeElimination) {
        // Check for unreachable code.
        for (size_t i = 0; i < cfg.fBlocks.size(); i++) {
            const BasicBlock& block = cfg.fBlocks[i];
            if (!block.fIsReachable && !block.fAllowUnreachable && block.fNodes.size()) {
                const BasicBlock::Node& node = block.fNodes[0];
                int offset = node.isStatement() ? (*node.statement())->fOffset
                                                : (*node.expression())->fOffset;
                this->error(offset, String("unreachable"));
            }
        }
    }
    if (fErrorCount) {
        return madeChanges;
    }

    // check for dead code & undefined variables, perform constant propagation
    OptimizationContext optimizationContext;
    optimizationContext.fUsage = usage;
    SkBitSet eliminatedBlockIds(cfg.fBlocks.size());
    do {
        if (optimizationContext.fNeedsRescan) {
            cfg = CFGGenerator().getCFG(f);
            this->computeDataFlow(&cfg);
            optimizationContext.fNeedsRescan = false;
        }

        eliminatedBlockIds.reset();
        optimizationContext.fUpdated = false;

        for (BlockId blockId = 0; blockId < cfg.fBlocks.size(); ++blockId) {
            if (eliminatedBlockIds.test(blockId)) {
                // We reached a block ID that might have been eliminated. Be cautious and rescan.
                optimizationContext.fUpdated = true;
                optimizationContext.fNeedsRescan = true;
                break;
            }

            BasicBlock& b = cfg.fBlocks[blockId];
            if (fContext->fConfig->fSettings.fDeadCodeElimination) {
                if (blockId > 0 && !b.fIsReachable) {
                    // Block was reachable before optimization, but has since become unreachable. In
                    // addition to being dead code, it's broken - since control flow can't reach it,
                    // no prior variable definitions can reach it, and therefore variables might
                    // look to have not been properly assigned. Kill it by replacing all statements
                    // with Nops.
                    for (BasicBlock::Node& node : b.fNodes) {
                        if (node.isStatement() && !(*node.statement())->is<Nop>()) {
                            // Eliminating a node runs the risk of eliminating that node's exits as
                            // well. Keep track of this and do a rescan if we are about to access
                            // one of these.
                            for (BlockId id : b.fExits) {
                                eliminatedBlockIds.set(id);
                            }
                            node.setStatement(std::make_unique<Nop>(), usage);
                            madeChanges = true;
                        }
                    }
                    continue;
                }
            }
            DefinitionMap definitions = b.fBefore;

            for (auto iter = b.fNodes.begin(); iter != b.fNodes.end() &&
                !optimizationContext.fNeedsRescan; ++iter) {
                if (iter->isExpression()) {
                    this->simplifyExpression(definitions, b, &iter, &optimizationContext);
                } else {
                    this->simplifyStatement(definitions, b, &iter, &optimizationContext);
                }
                if (optimizationContext.fNeedsRescan) {
                    break;
                }
                definitions.addDefinitions(*fContext, *iter);
            }

            if (optimizationContext.fNeedsRescan) {
                break;
            }
        }
        madeChanges |= optimizationContext.fUpdated;
    } while (optimizationContext.fUpdated);
    SkASSERT(!optimizationContext.fNeedsRescan);

    // check for missing return
    if (f.declaration().returnType() != *fContext->fTypes.fVoid) {
        if (cfg.fBlocks[cfg.fExit].fIsReachable) {
            this->error(f.fOffset, String("function '" + String(f.declaration().name()) +
                                          "' can exit without returning a value"));
        }
    }

    return madeChanges;
}

std::unique_ptr<Program> Compiler::convertProgram(
        ProgramKind kind,
        String text,
        const Program::Settings& settings,
        const std::vector<std::unique_ptr<ExternalFunction>>* externalFunctions) {
    TRACE_EVENT0("skia.gpu", "SkSL::Compiler::convertProgram");

    SkASSERT(!externalFunctions || (kind == ProgramKind::kGeneric));

    // Loading and optimizing our base module might reset the inliner, so do that first,
    // *then* configure the inliner with the settings for this program.
    const ParsedModule& baseModule = this->moduleForProgramKind(kind);

    // Update our context to point to the program configuration for the duration of compilation.
    auto config = std::make_unique<ProgramConfig>(ProgramConfig{kind, settings});

    SkASSERT(!fContext->fConfig);
    fContext->fConfig = config.get();
    SK_AT_SCOPE_EXIT(fContext->fConfig = nullptr);

    fErrorText = "";
    fErrorCount = 0;
    fInliner.reset(fIRGenerator->fModifiers.get());

    // Not using AutoSource, because caller is likely to call errorText() if we fail to compile
    std::unique_ptr<String> textPtr(new String(std::move(text)));
    fSource = textPtr.get();

    // Enable node pooling while converting and optimizing the program for a performance boost.
    // The Program will take ownership of the pool.
    std::unique_ptr<Pool> pool;
    if (fContext->fCaps.useNodePools()) {
        pool = Pool::Create();
        pool->attachToThread();
    }
    IRGenerator::IRBundle ir = fIRGenerator->convertProgram(baseModule, /*isBuiltinCode=*/false,
                                                            textPtr->c_str(), textPtr->size(),
                                                            externalFunctions);
    auto program = std::make_unique<Program>(std::move(textPtr),
                                             std::move(config),
                                             fContext,
                                             std::move(ir.fElements),
                                             std::move(ir.fSharedElements),
                                             std::move(ir.fModifiers),
                                             std::move(ir.fSymbolTable),
                                             std::move(pool),
                                             ir.fInputs);
    bool success = false;
    if (fErrorCount) {
        // Do not return programs that failed to compile.
    } else if (settings.fOptimize && !this->optimize(*program)) {
        // Do not return programs that failed to optimize.
    } else {
        // We have a successful program!
        success = true;
    }

    if (program->fPool) {
        program->fPool->detachFromThread();
    }
    return success ? std::move(program) : nullptr;
}

void Compiler::verifyStaticTests(const Program& program) {
    class StaticTestVerifier : public ProgramVisitor {
    public:
        StaticTestVerifier(ErrorReporter* r) : fReporter(r) {}

        using ProgramVisitor::visitProgramElement;

        bool visitStatement(const Statement& stmt) override {
            switch (stmt.kind()) {
                case Statement::Kind::kIf:
                    if (stmt.as<IfStatement>().isStatic()) {
                        fReporter->error(stmt.fOffset, "static if has non-static test");
                    }
                    break;

                case Statement::Kind::kSwitch:
                    if (stmt.as<SwitchStatement>().isStatic()) {
                        fReporter->error(stmt.fOffset, "static switch has non-static test");
                    }
                    break;

                default:
                    break;
            }
            return INHERITED::visitStatement(stmt);
        }

        bool visitExpression(const Expression&) override {
            // We aren't looking for anything inside an Expression, so skip them entirely.
            return false;
        }

    private:
        using INHERITED = ProgramVisitor;
        ErrorReporter* fReporter;
    };

    // If invalid static tests are permitted, we don't need to check anything.
    if (fContext->fConfig->fSettings.fPermitInvalidStaticTests) {
        return;
    }

    // Check all of the program's owned elements. (Built-in elements are assumed to be valid.)
    StaticTestVerifier visitor{this};
    for (const std::unique_ptr<ProgramElement>& element : program.ownedElements()) {
        if (element->is<FunctionDefinition>()) {
            visitor.visitProgramElement(*element);
        }
    }
}

bool Compiler::optimize(LoadedModule& module) {
    SkASSERT(!fErrorCount);

    // Create a temporary program configuration with default settings.
    ProgramConfig config;
    config.fKind = module.fKind;

    // Update our context to point to this configuration for the duration of compilation.
    SkASSERT(!fContext->fConfig);
    fContext->fConfig = &config;
    SK_AT_SCOPE_EXIT(fContext->fConfig = nullptr);

    // Reset the Inliner.
    fInliner.reset(fModifiers.back().get());

    std::unique_ptr<ProgramUsage> usage = Analysis::GetUsage(module);

    while (fErrorCount == 0) {
        bool madeChanges = false;

        // Scan and optimize based on the control-flow graph for each function.
        // TODO(skia:11365): we always perform CFG-based optimization here to reduce Settings into
        // their final form. We should do this optimization in our Make functions instead.
        for (const auto& element : module.fElements) {
            if (element->is<FunctionDefinition>()) {
                madeChanges |= this->scanCFG(element->as<FunctionDefinition>(), usage.get());
            }
        }

        // Perform inline-candidate analysis and inline any functions deemed suitable.
        madeChanges |= fInliner.analyze(module.fElements, module.fSymbols, usage.get());

        if (!madeChanges) {
            break;
        }
    }
    return fErrorCount == 0;
}

bool Compiler::optimize(Program& program) {
    SkASSERT(!fErrorCount);
    ProgramUsage* usage = program.fUsage.get();

    while (fErrorCount == 0) {
        bool madeChanges = false;

        // Scan and optimize based on the control-flow graph for each function.
        if (program.fConfig->fSettings.fControlFlowAnalysis) {
            for (const auto& element : program.ownedElements()) {
                if (element->is<FunctionDefinition>()) {
                    madeChanges |= this->scanCFG(element->as<FunctionDefinition>(), usage);
                }
            }
        }

        // Perform inline-candidate analysis and inline any functions deemed suitable.
        madeChanges |= fInliner.analyze(program.ownedElements(), program.fSymbols, usage);

        // Remove dead functions. We wait until after analysis so that we still report errors,
        // even in unused code.
        if (program.fConfig->fSettings.fRemoveDeadFunctions) {
            auto isDeadFunction = [&](const ProgramElement* element) {
                if (!element->is<FunctionDefinition>()) {
                    return false;
                }
                const FunctionDefinition& fn = element->as<FunctionDefinition>();
                if (fn.declaration().name() != "main" && usage->get(fn.declaration()) == 0) {
                    usage->remove(*element);
                    madeChanges = true;
                    return true;
                }
                return false;
            };
            program.fElements.erase(
                    std::remove_if(program.fElements.begin(), program.fElements.end(),
                                   [&](const std::unique_ptr<ProgramElement>& element) {
                                       return isDeadFunction(element.get());
                                   }),
                    program.fElements.end());
            program.fSharedElements.erase(
                    std::remove_if(program.fSharedElements.begin(), program.fSharedElements.end(),
                                   isDeadFunction),
                    program.fSharedElements.end());
        }

        if (program.fConfig->fKind != ProgramKind::kFragmentProcessor) {
            // Remove declarations of dead global variables
            auto isDeadVariable = [&](const ProgramElement* element) {
                if (!element->is<GlobalVarDeclaration>()) {
                    return false;
                }
                const GlobalVarDeclaration& global = element->as<GlobalVarDeclaration>();
                const VarDeclaration& varDecl = global.declaration()->as<VarDeclaration>();
                if (usage->isDead(varDecl.var())) {
                    madeChanges = true;
                    return true;
                }
                return false;
            };
            program.fElements.erase(
                    std::remove_if(program.fElements.begin(), program.fElements.end(),
                                   [&](const std::unique_ptr<ProgramElement>& element) {
                                       return isDeadVariable(element.get());
                                   }),
                    program.fElements.end());
            program.fSharedElements.erase(
                    std::remove_if(program.fSharedElements.begin(), program.fSharedElements.end(),
                                   isDeadVariable),
                    program.fSharedElements.end());
        }

        if (!madeChanges) {
            break;
        }
    }

    if (fErrorCount == 0) {
        this->verifyStaticTests(program);
    }

    return fErrorCount == 0;
}

#if defined(SKSL_STANDALONE) || SK_SUPPORT_GPU

bool Compiler::toSPIRV(Program& program, OutputStream& out) {
#ifdef SK_ENABLE_SPIRV_VALIDATION
    StringStream buffer;
    AutoSource as(this, program.fSource.get());
    SPIRVCodeGenerator cg(fContext.get(), &program, this, &buffer);
    bool result = cg.generateCode();
    if (result && program.fConfig->fSettings.fValidateSPIRV) {
        spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_0);
        const String& data = buffer.str();
        SkASSERT(0 == data.size() % 4);
        String errors;
        auto dumpmsg = [&errors](spv_message_level_t, const char*, const spv_position_t&,
                                 const char* m) {
            errors.appendf("SPIR-V validation error: %s\n", m);
        };
        tools.SetMessageConsumer(dumpmsg);

        // Verify that the SPIR-V we produced is valid. At runtime, we will abort() with a message
        // explaining the error. In standalone mode (skslc), we will send the message, plus the
        // entire disassembled SPIR-V (for easier context & debugging) as *our* error message.
        result = tools.Validate((const uint32_t*) data.c_str(), data.size() / 4);

        if (!result) {
#if defined(SKSL_STANDALONE)
            // Convert the string-stream to a SPIR-V disassembly.
            std::string disassembly;
            if (tools.Disassemble((const uint32_t*)data.data(), data.size() / 4, &disassembly)) {
                errors.append(disassembly);
            }
            this->error(-1, errors);
#else
            SkDEBUGFAILF("%s", errors.c_str());
#endif
        }
        out.write(data.c_str(), data.size());
    }
#else
    AutoSource as(this, program.fSource.get());
    SPIRVCodeGenerator cg(fContext.get(), &program, this, &out);
    bool result = cg.generateCode();
#endif
    return result;
}

bool Compiler::toSPIRV(Program& program, String* out) {
    StringStream buffer;
    bool result = this->toSPIRV(program, buffer);
    if (result) {
        *out = buffer.str();
    }
    return result;
}

bool Compiler::toGLSL(Program& program, OutputStream& out) {
    TRACE_EVENT0("skia.gpu", "SkSL::Compiler::toGLSL");
    AutoSource as(this, program.fSource.get());
    GLSLCodeGenerator cg(fContext.get(), &program, this, &out);
    bool result = cg.generateCode();
    return result;
}

bool Compiler::toGLSL(Program& program, String* out) {
    StringStream buffer;
    bool result = this->toGLSL(program, buffer);
    if (result) {
        *out = buffer.str();
    }
    return result;
}

bool Compiler::toHLSL(Program& program, String* out) {
    String spirv;
    if (!this->toSPIRV(program, &spirv)) {
        return false;
    }

    return SPIRVtoHLSL(spirv, out);
}

bool Compiler::toMetal(Program& program, OutputStream& out) {
    MetalCodeGenerator cg(fContext.get(), &program, this, &out);
    bool result = cg.generateCode();
    return result;
}

bool Compiler::toMetal(Program& program, String* out) {
    StringStream buffer;
    bool result = this->toMetal(program, buffer);
    if (result) {
        *out = buffer.str();
    }
    return result;
}

#if defined(SKSL_STANDALONE) || GR_TEST_UTILS
bool Compiler::toCPP(Program& program, String name, OutputStream& out) {
    AutoSource as(this, program.fSource.get());
    CPPCodeGenerator cg(fContext.get(), &program, this, name, &out);
    bool result = cg.generateCode();
    return result;
}

bool Compiler::toH(Program& program, String name, OutputStream& out) {
    AutoSource as(this, program.fSource.get());
    HCodeGenerator cg(fContext.get(), &program, this, name, &out);
    bool result = cg.generateCode();
    return result;
}
#endif // defined(SKSL_STANDALONE) || GR_TEST_UTILS

#endif // defined(SKSL_STANDALONE) || SK_SUPPORT_GPU

Position Compiler::position(int offset) {
    if (fSource && offset >= 0) {
        int line = 1;
        int column = 1;
        for (int i = 0; i < offset; i++) {
            if ((*fSource)[i] == '\n') {
                ++line;
                column = 1;
            }
            else {
                ++column;
            }
        }
        return Position(line, column);
    } else {
        return Position(-1, -1);
    }
}

void Compiler::error(int offset, String msg) {
    fErrorCount++;
    Position pos = this->position(offset);
    fErrorTextLength.push_back(fErrorText.length());
    fErrorText += "error: " + (pos.fLine >= 1 ? to_string(pos.fLine) + ": " : "") + msg + "\n";
}

void Compiler::setErrorCount(int c) {
    if (c < fErrorCount) {
        fErrorText.resize(fErrorTextLength[c]);
        fErrorTextLength.resize(c);
        fErrorCount = c;
    }
}

String Compiler::errorText(bool showCount) {
    if (showCount) {
        this->writeErrorCount();
    }
    fErrorCount = 0;
    String result = fErrorText;
    fErrorText = "";
    return result;
}

void Compiler::writeErrorCount() {
    if (fErrorCount) {
        fErrorText += to_string(fErrorCount) + " error";
        if (fErrorCount > 1) {
            fErrorText += "s";
        }
        fErrorText += "\n";
    }
}

}  // namespace SkSL
