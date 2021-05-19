//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl and feeds them into
//   glslang to spit out SPIR-V.
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/TranslatorVulkan.h"

#include "angle_gl.h"
#include "common/PackedEnums.h"
#include "common/utilities.h"
#include "compiler/translator/BuiltinsWorkaroundGLSL.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/OutputVulkanGLSL.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/glslang_wrapper.h"
#include "compiler/translator/tree_ops/vulkan/FlagSamplersWithTexelFetch.h"
#include "compiler/translator/tree_ops/vulkan/MonomorphizeUnsupportedFunctionsInVulkanGLSL.h"
#include "compiler/translator/tree_ops/vulkan/NameEmbeddedUniformStructs.h"
#include "compiler/translator/tree_ops/vulkan/RemoveAtomicCounterBuiltins.h"
#include "compiler/translator/tree_ops/vulkan/RemoveInactiveInterfaceVariables.h"
#include "compiler/translator/tree_ops/vulkan/ReplaceForShaderFramebufferFetch.h"
#include "compiler/translator/tree_ops/vulkan/RewriteArrayOfArrayOfOpaqueUniforms.h"
#include "compiler/translator/tree_ops/vulkan/RewriteAtomicCounters.h"
#include "compiler/translator/tree_ops/vulkan/RewriteCubeMapSamplersAs2DArray.h"
#include "compiler/translator/tree_ops/vulkan/RewriteDfdy.h"
#include "compiler/translator/tree_ops/vulkan/RewriteInterpolateAtOffset.h"
#include "compiler/translator/tree_ops/vulkan/RewriteR32fImages.h"
#include "compiler/translator/tree_ops/vulkan/RewriteStructSamplers.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/FindFunction.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceClipCullDistanceVariable.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RewriteSampleMaskVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"
#include "compiler/translator/tree_util/SpecializationConstant.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{
// This traverses nodes, find the struct ones and add their declarations to the sink. It also
// removes the nodes from the tree as it processes them.
class DeclareStructTypesTraverser : public TIntermTraverser
{
  public:
    explicit DeclareStructTypesTraverser(TOutputVulkanGLSL *outputVulkanGLSL)
        : TIntermTraverser(true, false, false), mOutputVulkanGLSL(outputVulkanGLSL)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        ASSERT(visit == PreVisit);

        if (!mInGlobalScope)
        {
            return false;
        }

        const TIntermSequence &sequence = *(node->getSequence());
        TIntermTyped *declarator        = sequence.front()->getAsTyped();
        const TType &type               = declarator->getType();

        if (type.isStructSpecifier())
        {
            const TStructure *structure = type.getStruct();

            // Embedded structs should be parsed away by now.
            ASSERT(structure->symbolType() != SymbolType::Empty);
            mOutputVulkanGLSL->writeStructType(structure);

            TIntermSymbol *symbolNode = declarator->getAsSymbolNode();
            if (symbolNode && symbolNode->variable().symbolType() == SymbolType::Empty)
            {
                // Remove the struct specifier declaration from the tree so it isn't parsed again.
                TIntermSequence emptyReplacement;
                mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                                std::move(emptyReplacement));
            }
        }

        return false;
    }

  private:
    TOutputVulkanGLSL *mOutputVulkanGLSL;
};

class DeclareDefaultUniformsTraverser : public TIntermTraverser
{
  public:
    DeclareDefaultUniformsTraverser(TInfoSinkBase *sink,
                                    ShHashFunction64 hashFunction,
                                    NameMap *nameMap)
        : TIntermTraverser(true, true, true),
          mSink(sink),
          mHashFunction(hashFunction),
          mNameMap(nameMap),
          mInDefaultUniform(false)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        // TODO(jmadill): Compound declarations.
        ASSERT(sequence.size() == 1);

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();
        bool isUniform         = type.getQualifier() == EvqUniform && !type.isInterfaceBlock() &&
                         !IsOpaqueType(type.getBasicType());

        if (visit == PreVisit)
        {
            if (isUniform)
            {
                (*mSink) << "    " << GetTypeName(type, mHashFunction, mNameMap) << " ";
                mInDefaultUniform = true;
            }
        }
        else if (visit == InVisit)
        {
            mInDefaultUniform = isUniform;
        }
        else if (visit == PostVisit)
        {
            if (isUniform)
            {
                (*mSink) << ";\n";

                // Remove the uniform declaration from the tree so it isn't parsed again.
                TIntermSequence emptyReplacement;
                mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                                std::move(emptyReplacement));
            }

            mInDefaultUniform = false;
        }
        return true;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        if (mInDefaultUniform)
        {
            const ImmutableString &name = symbol->variable().name();
            ASSERT(!name.beginsWith("gl_"));
            (*mSink) << HashName(&symbol->variable(), mHashFunction, mNameMap)
                     << ArrayString(symbol->getType());
        }
    }

  private:
    TInfoSinkBase *mSink;
    ShHashFunction64 mHashFunction;
    NameMap *mNameMap;
    bool mInDefaultUniform;
};

constexpr ImmutableString kFlippedPointCoordName = ImmutableString("flippedPointCoord");
constexpr ImmutableString kFlippedFragCoordName  = ImmutableString("flippedFragCoord");

constexpr gl::ShaderMap<const char *> kDefaultUniformNames = {
    {gl::ShaderType::Vertex, vk::kDefaultUniformsNameVS},
    {gl::ShaderType::TessControl, vk::kDefaultUniformsNameTCS},
    {gl::ShaderType::TessEvaluation, vk::kDefaultUniformsNameTES},
    {gl::ShaderType::Geometry, vk::kDefaultUniformsNameGS},
    {gl::ShaderType::Fragment, vk::kDefaultUniformsNameFS},
    {gl::ShaderType::Compute, vk::kDefaultUniformsNameCS},
};

// Replaces a builtin variable with a version that is rotated and corrects the X and Y coordinates.
ANGLE_NO_DISCARD bool RotateAndFlipBuiltinVariable(TCompiler *compiler,
                                                   TIntermBlock *root,
                                                   TIntermSequence *insertSequence,
                                                   TIntermTyped *flipXY,
                                                   TSymbolTable *symbolTable,
                                                   const TVariable *builtin,
                                                   const ImmutableString &flippedVariableName,
                                                   TIntermTyped *pivot,
                                                   TIntermTyped *fragRotation)
{
    // Create a symbol reference to 'builtin'.
    TIntermSymbol *builtinRef = new TIntermSymbol(builtin);

    // Create a swizzle to "builtin.xy"
    TVector<int> swizzleOffsetXY = {0, 1};
    TIntermSwizzle *builtinXY    = new TIntermSwizzle(builtinRef, swizzleOffsetXY);

    // Create a symbol reference to our new variable that will hold the modified builtin.
    const TType *type = StaticType::GetForVec<EbtFloat>(
        EvqGlobal, static_cast<unsigned char>(builtin->getType().getNominalSize()));
    TVariable *replacementVar =
        new TVariable(symbolTable, flippedVariableName, type, SymbolType::AngleInternal);
    DeclareGlobalVariable(root, replacementVar);
    TIntermSymbol *flippedBuiltinRef = new TIntermSymbol(replacementVar);

    // Use this new variable instead of 'builtin' everywhere.
    if (!ReplaceVariable(compiler, root, builtin, replacementVar))
    {
        return false;
    }

    // Create the expression "(builtin.xy * fragRotation)"
    TIntermTyped *rotatedXY;
    if (fragRotation)
    {
        rotatedXY = new TIntermBinary(EOpMatrixTimesVector, fragRotation, builtinXY);
    }
    else
    {
        // No rotation applied, use original variable.
        rotatedXY = builtinXY;
    }

    // Create the expression "(builtin.xy - pivot) * flipXY + pivot
    TIntermBinary *removePivot = new TIntermBinary(EOpSub, rotatedXY, pivot);
    TIntermBinary *inverseXY   = new TIntermBinary(EOpMul, removePivot, flipXY);
    TIntermBinary *plusPivot   = new TIntermBinary(EOpAdd, inverseXY, pivot->deepCopy());

    // Create the corrected variable and copy the value of the original builtin.
    TIntermSequence sequence;
    sequence.push_back(builtinRef->deepCopy());
    TIntermAggregate *aggregate =
        TIntermAggregate::CreateConstructor(builtin->getType(), &sequence);
    TIntermBinary *assignment = new TIntermBinary(EOpInitialize, flippedBuiltinRef, aggregate);

    // Create an assignment to the replaced variable's .xy.
    TIntermSwizzle *correctedXY =
        new TIntermSwizzle(flippedBuiltinRef->deepCopy(), swizzleOffsetXY);
    TIntermBinary *assignToY = new TIntermBinary(EOpAssign, correctedXY, plusPivot);

    // Add this assigment at the beginning of the main function
    insertSequence->insert(insertSequence->begin(), assignToY);
    insertSequence->insert(insertSequence->begin(), assignment);

    return compiler->validateAST(root);
}

TIntermSequence *GetMainSequence(TIntermBlock *root)
{
    TIntermFunctionDefinition *main = FindMain(root);
    return main->getBody()->getSequence();
}

// Declares a new variable to replace gl_DepthRange, its values are fed from a driver uniform.
ANGLE_NO_DISCARD bool ReplaceGLDepthRangeWithDriverUniform(TCompiler *compiler,
                                                           TIntermBlock *root,
                                                           const DriverUniform *driverUniforms,
                                                           TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_DepthRange"
    const TVariable *depthRangeVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_DepthRange"), 0));

    // ANGLEUniforms.depthRange
    TIntermBinary *angleEmulatedDepthRangeRef = driverUniforms->getDepthRangeRef();

    // Use this variable instead of gl_DepthRange everywhere.
    return ReplaceVariableWithTyped(compiler, root, depthRangeVar, angleEmulatedDepthRangeRef);
}

TVariable *AddANGLEPositionVaryingDeclaration(TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              TQualifier qualifier)
{
    // Define a vec2 driver varying to hold the line rasterization emulation position.
    TType *varyingType = new TType(EbtFloat, EbpMedium, qualifier, 2);
    TVariable *varyingVar =
        new TVariable(symbolTable, ImmutableString(vk::kLineRasterEmulationPosition), varyingType,
                      SymbolType::AngleInternal);
    TIntermSymbol *varyingDeclarator = new TIntermSymbol(varyingVar);
    TIntermDeclaration *varyingDecl  = new TIntermDeclaration;
    varyingDecl->appendDeclarator(varyingDeclarator);

    TIntermSequence insertSequence;
    insertSequence.push_back(varyingDecl);

    // Insert the declarations before Main.
    size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, insertSequence);

    return varyingVar;
}

ANGLE_NO_DISCARD bool AddBresenhamEmulationVS(TCompiler *compiler,
                                              TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              SpecConst *specConst,
                                              const DriverUniform *driverUniforms)
{
    TVariable *anglePosition = AddANGLEPositionVaryingDeclaration(root, symbolTable, EvqVaryingOut);

    // Clamp position to subpixel grid.
    // Do perspective divide (get normalized device coords)
    // "vec2 ndc = gl_Position.xy / gl_Position.w"
    const TType *vec2Type        = StaticType::GetBasic<EbtFloat, 2>();
    TIntermBinary *viewportRef   = driverUniforms->getViewportRef();
    TIntermSymbol *glPos         = new TIntermSymbol(BuiltInVariable::gl_Position());
    TIntermSwizzle *glPosXY      = CreateSwizzle(glPos, 0, 1);
    TIntermSwizzle *glPosW       = CreateSwizzle(glPos->deepCopy(), 3);
    TVariable *ndc               = CreateTempVariable(symbolTable, vec2Type);
    TIntermBinary *noPerspective = new TIntermBinary(EOpDiv, glPosXY, glPosW);
    TIntermDeclaration *ndcDecl  = CreateTempInitDeclarationNode(ndc, noPerspective);

    // Convert NDC to window coordinates. According to Vulkan spec.
    // "vec2 window = 0.5 * viewport.wh * (ndc + 1) + viewport.xy"
    TIntermBinary *ndcPlusOne =
        new TIntermBinary(EOpAdd, CreateTempSymbolNode(ndc), CreateFloatNode(1.0f));
    TIntermSwizzle *viewportZW = CreateSwizzle(viewportRef, 2, 3);
    TIntermBinary *ndcViewport = new TIntermBinary(EOpMul, viewportZW, ndcPlusOne);
    TIntermBinary *ndcViewportHalf =
        new TIntermBinary(EOpVectorTimesScalar, ndcViewport, CreateFloatNode(0.5f));
    TIntermSwizzle *viewportXY     = CreateSwizzle(viewportRef->deepCopy(), 0, 1);
    TIntermBinary *ndcToWindow     = new TIntermBinary(EOpAdd, ndcViewportHalf, viewportXY);
    TVariable *windowCoords        = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *windowDecl = CreateTempInitDeclarationNode(windowCoords, ndcToWindow);

    // Clamp to subpixel grid.
    // "vec2 clamped = round(window * 2^{subpixelBits}) / 2^{subpixelBits}"
    int subpixelBits                    = compiler->getResources().SubPixelBits;
    TIntermConstantUnion *scaleConstant = CreateFloatNode(static_cast<float>(1 << subpixelBits));
    TIntermBinary *windowScaled =
        new TIntermBinary(EOpVectorTimesScalar, CreateTempSymbolNode(windowCoords), scaleConstant);
    TIntermUnary *windowRounded = new TIntermUnary(EOpRound, windowScaled, nullptr);
    TIntermBinary *windowRoundedBack =
        new TIntermBinary(EOpDiv, windowRounded, scaleConstant->deepCopy());
    TVariable *clampedWindowCoords = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *clampedDecl =
        CreateTempInitDeclarationNode(clampedWindowCoords, windowRoundedBack);

    // Set varying.
    // "ANGLEPosition = 2 * (clamped - viewport.xy) / viewport.wh - 1"
    TIntermBinary *clampedOffset = new TIntermBinary(
        EOpSub, CreateTempSymbolNode(clampedWindowCoords), viewportXY->deepCopy());
    TIntermBinary *clampedOff2x =
        new TIntermBinary(EOpVectorTimesScalar, clampedOffset, CreateFloatNode(2.0f));
    TIntermBinary *clampedDivided = new TIntermBinary(EOpDiv, clampedOff2x, viewportZW->deepCopy());
    TIntermBinary *clampedNDC    = new TIntermBinary(EOpSub, clampedDivided, CreateFloatNode(1.0f));
    TIntermSymbol *varyingRef    = new TIntermSymbol(anglePosition);
    TIntermBinary *varyingAssign = new TIntermBinary(EOpAssign, varyingRef, clampedNDC);

    TIntermBlock *emulationBlock = new TIntermBlock;
    emulationBlock->appendStatement(ndcDecl);
    emulationBlock->appendStatement(windowDecl);
    emulationBlock->appendStatement(clampedDecl);
    emulationBlock->appendStatement(varyingAssign);
    TIntermIfElse *ifEmulation =
        new TIntermIfElse(specConst->getLineRasterEmulation(), emulationBlock, nullptr);

    // Ensure the statements run at the end of the main() function.
    TIntermFunctionDefinition *main = FindMain(root);
    TIntermBlock *mainBody          = main->getBody();
    mainBody->appendStatement(ifEmulation);
    return compiler->validateAST(root);
}

ANGLE_NO_DISCARD bool AddXfbEmulationSupport(TCompiler *compiler,
                                             TIntermBlock *root,
                                             TSymbolTable *symbolTable,
                                             const DriverUniform *driverUniforms)
{
    // Generate the following function and place it before main().  This function takes a "strides"
    // parameter that is determined at link time, and calculates for each transform feedback buffer
    // (of which there are a maximum of four) what the starting index is to write to the output
    // buffer.
    //
    //     ivec4 ANGLEGetXfbOffsets(ivec4 strides)
    //     {
    //         int xfbIndex = gl_VertexIndex
    //                      + gl_InstanceIndex * ANGLEUniforms.xfbVerticesPerInstance;
    //         return ANGLEUniforms.xfbBufferOffsets + xfbIndex * strides;
    //     }

    constexpr uint32_t kMaxXfbBuffers = 4;

    const TType *ivec4Type = StaticType::GetBasic<EbtInt, kMaxXfbBuffers>();

    // Create the parameter variable.
    TVariable *stridesVar        = new TVariable(symbolTable, ImmutableString("strides"), ivec4Type,
                                          SymbolType::AngleInternal);
    TIntermSymbol *stridesSymbol = new TIntermSymbol(stridesVar);

    // Create references to gl_VertexIndex, gl_InstanceIndex, ANGLEUniforms.xfbVerticesPerInstance
    // and ANGLEUniforms.xfbBufferOffsets.
    TIntermSymbol *vertexIndex            = new TIntermSymbol(BuiltInVariable::gl_VertexIndex());
    TIntermSymbol *instanceIndex          = new TIntermSymbol(BuiltInVariable::gl_InstanceIndex());
    TIntermBinary *xfbVerticesPerInstance = driverUniforms->getXfbVerticesPerInstance();
    TIntermBinary *xfbBufferOffsets       = driverUniforms->getXfbBufferOffsets();

    // gl_InstanceIndex * ANGLEUniforms.xfbVerticesPerInstance
    TIntermBinary *xfbInstanceIndex =
        new TIntermBinary(EOpMul, instanceIndex, xfbVerticesPerInstance);

    // gl_VertexIndex + |xfbInstanceIndex|
    TIntermBinary *xfbIndex = new TIntermBinary(EOpAdd, vertexIndex, xfbInstanceIndex);

    // |xfbIndex| * |strides|
    TIntermBinary *xfbStrides = new TIntermBinary(EOpVectorTimesScalar, xfbIndex, stridesSymbol);

    // ANGLEUniforms.xfbBufferOffsets + |xfbStrides|
    TIntermBinary *xfbOffsets = new TIntermBinary(EOpAdd, xfbBufferOffsets, xfbStrides);

    // Create the function body, which has a single return statement.  Note that the `xfbIndex`
    // variable declared in the comment at the beginning of this function is simply replaced in the
    // return statement for brevity.
    TIntermBlock *body = new TIntermBlock;
    body->appendStatement(new TIntermBranch(EOpReturn, xfbOffsets));

    // Declare the function
    TFunction *getOffsetsFunction =
        new TFunction(symbolTable, ImmutableString(vk::kXfbEmulationGetOffsetsFunctionName),
                      SymbolType::AngleInternal, ivec4Type, true);
    getOffsetsFunction->addParameter(stridesVar);

    TIntermFunctionDefinition *functionDef =
        CreateInternalFunctionDefinitionNode(*getOffsetsFunction, body);

    // Insert the function declaration before main().
    const size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, {functionDef});

    // Generate the following function and place it before main().  This function will be filled
    // with transform feedback capture code at link time.
    //
    //     void ANGLECaptureXfb()
    //     {
    //     }
    const TType *voidType = StaticType::GetBasic<EbtVoid>();

    // Create the function body, which is empty.
    body = new TIntermBlock;

    // Declare the function
    TFunction *xfbCaptureFunction =
        new TFunction(symbolTable, ImmutableString(vk::kXfbEmulationCaptureFunctionName),
                      SymbolType::AngleInternal, voidType, false);

    // Insert the function declaration before main().
    root->insertChildNodes(mainIndex,
                           {CreateInternalFunctionDefinitionNode(*xfbCaptureFunction, body)});

    // Create the following logic and add it at the end of main():
    //
    //     if (ANGLEUniforms.xfbActiveUnpaused)
    //     {
    //         ANGLECaptureXfb();
    //     }
    //

    // Create a reference ANGLEUniforms.xfbActiveUnpaused
    TIntermBinary *xfbActiveUnpaused = driverUniforms->getXfbActiveUnpaused();

    // ANGLEUniforms.xfbActiveUnpaused != 0
    TIntermBinary *isXfbActiveUnpaused =
        new TIntermBinary(EOpNotEqual, xfbActiveUnpaused, CreateUIntNode(0));

    // Create the function call
    TIntermAggregate *captureXfbCall =
        TIntermAggregate::CreateFunctionCall(*xfbCaptureFunction, {});

    TIntermBlock *captureXfbBlock = new TIntermBlock;
    captureXfbBlock->appendStatement(captureXfbCall);

    // Create a call to ANGLEGetXfbOffsets too, for the sole purpose of preventing it from being
    // culled as unused by glslang.
    TIntermSequence zero;
    zero.push_back(CreateIndexNode(0));
    TIntermSequence ivec4Zero;
    ivec4Zero.push_back(TIntermAggregate::CreateConstructor(*ivec4Type, &zero));
    TIntermAggregate *getOffsetsCall =
        TIntermAggregate::CreateFunctionCall(*getOffsetsFunction, &ivec4Zero);
    captureXfbBlock->appendStatement(getOffsetsCall);

    // Create the if
    TIntermIfElse *captureXfb = new TIntermIfElse(isXfbActiveUnpaused, captureXfbBlock, nullptr);

    // Run it at the end of the shader.
    if (!RunAtTheEndOfShader(compiler, root, captureXfb, symbolTable))
    {
        return false;
    }

    // Additionally, generate the following storage buffer declarations used to capture transform
    // feedback output.  Again, there's a maximum of four buffers.
    //
    //     buffer ANGLEXfbBuffer0
    //     {
    //         float xfbOut[];
    //     } ANGLEXfb0;
    //     buffer ANGLEXfbBuffer1
    //     {
    //         float xfbOut[];
    //     } ANGLEXfb1;
    //     ...

    for (uint32_t bufferIndex = 0; bufferIndex < kMaxXfbBuffers; ++bufferIndex)
    {
        TFieldList *fieldList = new TFieldList;
        TType *xfbOutType     = new TType(EbtFloat);
        xfbOutType->makeArray(0);

        TField *field = new TField(xfbOutType, ImmutableString(vk::kXfbEmulationBufferFieldName),
                                   TSourceLoc(), SymbolType::AngleInternal);

        fieldList->push_back(field);

        static_assert(
            kMaxXfbBuffers < 10,
            "ImmutableStringBuilder memory size below needs to accomodate the number of buffers");

        ImmutableStringBuilder blockName(strlen(vk::kXfbEmulationBufferBlockName) + 2);
        blockName << vk::kXfbEmulationBufferBlockName;
        blockName.appendDecimal(bufferIndex);

        ImmutableStringBuilder varName(strlen(vk::kXfbEmulationBufferName) + 2);
        varName << vk::kXfbEmulationBufferName;
        varName.appendDecimal(bufferIndex);

        DeclareInterfaceBlock(root, symbolTable, fieldList, EvqBuffer, TMemoryQualifier::Create(),
                              0, blockName, varName);
    }

    return compiler->validateAST(root);
}

ANGLE_NO_DISCARD bool AddXfbExtensionSupport(TCompiler *compiler,
                                             TIntermBlock *root,
                                             TSymbolTable *symbolTable,
                                             const DriverUniform *driverUniforms)
{
    // Generate the following output varying declaration used to capture transform feedback output
    // from gl_Position, as it can't be captured directly due to changes that are applied to it for
    // clip-space correction and pre-rotation.
    //
    //     out vec4 ANGLEXfbPosition;

    const TType *vec4Type = nullptr;

    switch (compiler->getShaderType())
    {
        case GL_VERTEX_SHADER:
            vec4Type = StaticType::Get<EbtFloat, EbpHigh, EvqVertexOut, 4, 1>();
            break;
        case GL_TESS_EVALUATION_SHADER_EXT:
            vec4Type = StaticType::Get<EbtFloat, EbpHigh, EvqTessEvaluationOut, 4, 1>();
            break;
        case GL_GEOMETRY_SHADER_EXT:
            vec4Type = StaticType::Get<EbtFloat, EbpHigh, EvqGeometryOut, 4, 1>();
            break;
        default:
            UNREACHABLE();
    }

    TVariable *varyingVar =
        new TVariable(symbolTable, ImmutableString(vk::kXfbExtensionPositionOutName), vec4Type,
                      SymbolType::AngleInternal);

    TIntermDeclaration *varyingDecl = new TIntermDeclaration();
    varyingDecl->appendDeclarator(new TIntermSymbol(varyingVar));

    // Insert the varying declaration before the first function.
    const size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
    root->insertChildNodes(firstFunctionIndex, {varyingDecl});

    return compiler->validateAST(root);
}

ANGLE_NO_DISCARD bool InsertFragCoordCorrection(TCompiler *compiler,
                                                ShCompileOptions compileOptions,
                                                TIntermBlock *root,
                                                TIntermSequence *insertSequence,
                                                TSymbolTable *symbolTable,
                                                SpecConst *specConst,
                                                const DriverUniform *driverUniforms)
{
    TIntermTyped *flipXY = specConst->getFlipXY();
    if (!flipXY)
    {
        flipXY = driverUniforms->getFlipXYRef();
    }

    TIntermBinary *pivot = specConst->getHalfRenderArea();
    if (!pivot)
    {
        pivot = driverUniforms->getHalfRenderAreaRef();
    }

    TIntermTyped *fragRotation = nullptr;
    if ((compileOptions & SH_ADD_PRE_ROTATION) != 0)
    {
        fragRotation = specConst->getFragRotationMatrix();
        if (!fragRotation)
        {
            fragRotation = driverUniforms->getFragRotationMatrixRef();
        }
    }
    return RotateAndFlipBuiltinVariable(compiler, root, insertSequence, flipXY, symbolTable,
                                        BuiltInVariable::gl_FragCoord(), kFlippedFragCoordName,
                                        pivot, fragRotation);
}

// This block adds OpenGL line segment rasterization emulation behind a specialization constant
// guard.  OpenGL's simple rasterization algorithm is a strict subset of the pixels generated by the
// Vulkan algorithm. Thus we can implement a shader patch that rejects pixels if they would not be
// generated by the OpenGL algorithm. OpenGL's algorithm is similar to Bresenham's line algorithm.
// It is implemented for each pixel by testing if the line segment crosses a small diamond inside
// the pixel. See the OpenGL ES 2.0 spec section "3.4.1 Basic Line Segment Rasterization". Also
// see the Vulkan spec section "24.6.1. Basic Line Segment Rasterization":
// https://khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#primsrast-lines-basic
//
// Using trigonometric math and the fact that we know the size of the diamond we can derive a
// formula to test if the line segment crosses the pixel center. gl_FragCoord is used along with an
// internal position varying to determine the inputs to the formula.
//
// The implementation of the test is similar to the following pseudocode:
//
// void main()
// {
//    vec2 p  = (((((ANGLEPosition.xy) * 0.5) + 0.5) * viewport.zw) + viewport.xy);
//    vec2 d  = dFdx(p) + dFdy(p);
//    vec2 f  = gl_FragCoord.xy;
//    vec2 p_ = p.yx;
//    vec2 d_ = d.yx;
//    vec2 f_ = f.yx;
//
//    vec2 i = abs(p - f + (d / d_) * (f_ - p_));
//
//    if (i.x > (0.5 + e) && i.y > (0.5 + e))
//        discard;
//     <otherwise run fragment shader main>
// }
//
// Note this emulation can not provide fully correct rasterization. See the docs more more info.

ANGLE_NO_DISCARD bool AddBresenhamEmulationFS(TCompiler *compiler,
                                              ShCompileOptions compileOptions,
                                              TInfoSinkBase &sink,
                                              TIntermBlock *root,
                                              TSymbolTable *symbolTable,
                                              SpecConst *specConst,
                                              const DriverUniform *driverUniforms,
                                              bool usesFragCoord)
{
    TVariable *anglePosition = AddANGLEPositionVaryingDeclaration(root, symbolTable, EvqVaryingIn);
    const TType *vec2Type    = StaticType::GetBasic<EbtFloat, 2>();
    TIntermBinary *viewportRef = driverUniforms->getViewportRef();

    // vec2 p = ((ANGLEPosition * 0.5) + 0.5) * viewport.zw + viewport.xy
    TIntermSwizzle *viewportXY    = CreateSwizzle(viewportRef->deepCopy(), 0, 1);
    TIntermSwizzle *viewportZW    = CreateSwizzle(viewportRef, 2, 3);
    TIntermSymbol *position       = new TIntermSymbol(anglePosition);
    TIntermConstantUnion *oneHalf = CreateFloatNode(0.5f);
    TIntermBinary *halfPosition   = new TIntermBinary(EOpVectorTimesScalar, position, oneHalf);
    TIntermBinary *offsetHalfPosition =
        new TIntermBinary(EOpAdd, halfPosition, oneHalf->deepCopy());
    TIntermBinary *scaledPosition = new TIntermBinary(EOpMul, offsetHalfPosition, viewportZW);
    TIntermBinary *windowPosition = new TIntermBinary(EOpAdd, scaledPosition, viewportXY);
    TVariable *p                  = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *pDecl     = CreateTempInitDeclarationNode(p, windowPosition);

    // vec2 d = dFdx(p) + dFdy(p)
    TIntermUnary *dfdx        = new TIntermUnary(EOpDFdx, new TIntermSymbol(p), nullptr);
    TIntermUnary *dfdy        = new TIntermUnary(EOpDFdy, new TIntermSymbol(p), nullptr);
    TIntermBinary *dfsum      = new TIntermBinary(EOpAdd, dfdx, dfdy);
    TVariable *d              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *dDecl = CreateTempInitDeclarationNode(d, dfsum);

    // vec2 f = gl_FragCoord.xy
    const TVariable *fragCoord  = BuiltInVariable::gl_FragCoord();
    TIntermSwizzle *fragCoordXY = CreateSwizzle(new TIntermSymbol(fragCoord), 0, 1);
    TVariable *f                = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *fDecl   = CreateTempInitDeclarationNode(f, fragCoordXY);

    // vec2 p_ = p.yx
    TIntermSwizzle *pyx        = CreateSwizzle(new TIntermSymbol(p), 1, 0);
    TVariable *p_              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *p_decl = CreateTempInitDeclarationNode(p_, pyx);

    // vec2 d_ = d.yx
    TIntermSwizzle *dyx        = CreateSwizzle(new TIntermSymbol(d), 1, 0);
    TVariable *d_              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *d_decl = CreateTempInitDeclarationNode(d_, dyx);

    // vec2 f_ = f.yx
    TIntermSwizzle *fyx        = CreateSwizzle(new TIntermSymbol(f), 1, 0);
    TVariable *f_              = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *f_decl = CreateTempInitDeclarationNode(f_, fyx);

    // vec2 i = abs(p - f + (d/d_) * (f_ - p_))
    TIntermBinary *dd   = new TIntermBinary(EOpDiv, new TIntermSymbol(d), new TIntermSymbol(d_));
    TIntermBinary *fp   = new TIntermBinary(EOpSub, new TIntermSymbol(f_), new TIntermSymbol(p_));
    TIntermBinary *ddfp = new TIntermBinary(EOpMul, dd, fp);
    TIntermBinary *pf   = new TIntermBinary(EOpSub, new TIntermSymbol(p), new TIntermSymbol(f));
    TIntermBinary *expr = new TIntermBinary(EOpAdd, pf, ddfp);
    TIntermUnary *absd  = new TIntermUnary(EOpAbs, expr, nullptr);
    TVariable *i        = CreateTempVariable(symbolTable, vec2Type);
    TIntermDeclaration *iDecl = CreateTempInitDeclarationNode(i, absd);

    // Using a small epsilon value ensures that we don't suffer from numerical instability when
    // lines are exactly vertical or horizontal.
    static constexpr float kEpsilon   = 0.0001f;
    static constexpr float kThreshold = 0.5 + kEpsilon;
    TIntermConstantUnion *threshold   = CreateFloatNode(kThreshold);

    // if (i.x > (0.5 + e) && i.y > (0.5 + e))
    TIntermSwizzle *ix     = CreateSwizzle(new TIntermSymbol(i), 0);
    TIntermBinary *checkX  = new TIntermBinary(EOpGreaterThan, ix, threshold);
    TIntermSwizzle *iy     = CreateSwizzle(new TIntermSymbol(i), 1);
    TIntermBinary *checkY  = new TIntermBinary(EOpGreaterThan, iy, threshold->deepCopy());
    TIntermBinary *checkXY = new TIntermBinary(EOpLogicalAnd, checkX, checkY);

    // discard
    TIntermBranch *discard     = new TIntermBranch(EOpKill, nullptr);
    TIntermBlock *discardBlock = new TIntermBlock;
    discardBlock->appendStatement(discard);
    TIntermIfElse *ifStatement = new TIntermIfElse(checkXY, discardBlock, nullptr);

    TIntermBlock *emulationBlock       = new TIntermBlock;
    TIntermSequence *emulationSequence = emulationBlock->getSequence();

    std::array<TIntermNode *, 8> nodes = {
        {pDecl, dDecl, fDecl, p_decl, d_decl, f_decl, iDecl, ifStatement}};
    emulationSequence->insert(emulationSequence->begin(), nodes.begin(), nodes.end());

    TIntermIfElse *ifEmulation =
        new TIntermIfElse(specConst->getLineRasterEmulation(), emulationBlock, nullptr);

    // Ensure the line raster code runs at the beginning of main().
    TIntermFunctionDefinition *main = FindMain(root);
    TIntermSequence *mainSequence   = main->getBody()->getSequence();
    ASSERT(mainSequence);

    mainSequence->insert(mainSequence->begin(), ifEmulation);

    // If the shader does not use frag coord, we should insert it inside the emulation if.
    if (!usesFragCoord)
    {
        if (!InsertFragCoordCorrection(compiler, compileOptions, root, emulationSequence,
                                       symbolTable, specConst, driverUniforms))
        {
            return false;
        }
    }

    return compiler->validateAST(root);
}
}  // anonymous namespace

TranslatorVulkan::TranslatorVulkan(sh::GLenum type, ShShaderSpec spec)
    : TCompiler(type, spec, SH_GLSL_450_CORE_OUTPUT)
{}

bool TranslatorVulkan::translateImpl(TInfoSinkBase &sink,
                                     TIntermBlock *root,
                                     ShCompileOptions compileOptions,
                                     PerformanceDiagnostics * /*perfDiagnostics*/,
                                     SpecConst *specConst,
                                     DriverUniform *driverUniforms,
                                     TOutputVulkanGLSL *outputGLSL)
{
    if (getShaderType() == GL_VERTEX_SHADER)
    {
        if (!ShaderBuiltinsWorkaround(this, root, &getSymbolTable(), compileOptions))
        {
            return false;
        }
    }

    sink << "#version 450 core\n";

    // Write out default uniforms into a uniform block assigned to a specific set/binding.
    int defaultUniformCount           = 0;
    int aggregateTypesUsedForUniforms = 0;
    int r32fImageCount                = 0;
    int atomicCounterCount            = 0;
    for (const auto &uniform : getUniforms())
    {
        if (!uniform.isBuiltIn() && uniform.active && !gl::IsOpaqueType(uniform.type))
        {
            ++defaultUniformCount;
        }

        if (uniform.isStruct() || uniform.isArrayOfArrays())
        {
            ++aggregateTypesUsedForUniforms;
        }

        if (uniform.active && gl::IsImageType(uniform.type) && uniform.imageUnitFormat == GL_R32F)
        {
            ++r32fImageCount;
        }

        if (uniform.active && gl::IsAtomicCounterType(uniform.type))
        {
            ++atomicCounterCount;
        }
    }

    // Remove declarations of inactive shader interface variables so glslang wrapper doesn't need to
    // replace them.  Note: this is done before extracting samplers from structs, as removing such
    // inactive samplers is not yet supported.  Note also that currently, CollectVariables marks
    // every field of an active uniform that's of struct type as active, i.e. no extracted sampler
    // is inactive.
    if (!RemoveInactiveInterfaceVariables(this, root, getAttributes(), getInputVaryings(),
                                          getOutputVariables(), getUniforms(),
                                          getInterfaceBlocks()))
    {
        return false;
    }

    // If there are any function calls that take array-of-array of opaque uniform parameters, or
    // other opaque uniforms that need special handling in Vulkan, such as atomic counters,
    // monomorphize the functions by removing said parameters and replacing them in the function
    // body with the call arguments.
    //
    // This has a few benefits:
    //
    // - It dramatically simplifies future transformations w.r.t to samplers in structs, array of
    //   arrays of opaque types, atomic counters etc.
    // - Avoids the need for shader*ArrayDynamicIndexing Vulkan features.
    if (!MonomorphizeUnsupportedFunctionsInVulkanGLSL(this, root, &getSymbolTable(),
                                                      compileOptions))
    {
        return false;
    }

    if (aggregateTypesUsedForUniforms > 0)
    {
        if (!NameEmbeddedStructUniforms(this, root, &getSymbolTable()))
        {
            return false;
        }

        int removedUniformsCount;

        if (!RewriteStructSamplers(this, root, &getSymbolTable(), &removedUniformsCount))
        {
            return false;
        }
        defaultUniformCount -= removedUniformsCount;

        // We must declare the struct types before using them.
        DeclareStructTypesTraverser structTypesTraverser(outputGLSL);
        root->traverse(&structTypesTraverser);
        if (!structTypesTraverser.updateTree(this, root))
        {
            return false;
        }
    }

    // Replace array of array of opaque uniforms with a flattened array.  This is run after
    // MonomorphizeUnsupportedFunctionsInVulkanGLSL and RewriteStructSamplers so that it's not
    // possible for an array of array of opaque type to be partially subscripted and passed to a
    // function.
    if (!RewriteArrayOfArrayOfOpaqueUniforms(this, root, &getSymbolTable()))
    {
        return false;
    }

    // Rewrite samplerCubes as sampler2DArrays.  This must be done after rewriting struct samplers
    // as it doesn't expect that.
    if ((compileOptions & SH_EMULATE_SEAMFUL_CUBE_MAP_SAMPLING) != 0)
    {
        if (!RewriteCubeMapSamplersAs2DArray(this, root, &getSymbolTable(),
                                             getShaderType() == GL_FRAGMENT_SHADER))
        {
            return false;
        }
    }

    if (!FlagSamplersForTexelFetch(this, root, &getSymbolTable(), &mUniforms))
    {
        return false;
    }

    gl::ShaderType packedShaderType = gl::FromGLenum<gl::ShaderType>(getShaderType());

    if (defaultUniformCount > 0)
    {
        sink << "\nlayout(set=0, binding=" << outputGLSL->nextUnusedBinding()
             << ", std140) uniform " << kDefaultUniformNames[packedShaderType] << "\n{\n";

        DeclareDefaultUniformsTraverser defaultTraverser(&sink, getHashFunction(), &getNameMap());
        root->traverse(&defaultTraverser);
        if (!defaultTraverser.updateTree(this, root))
        {
            return false;
        }

        sink << "};\n";
    }

    if (getShaderType() == GL_COMPUTE_SHADER)
    {
        driverUniforms->addComputeDriverUniformsToShader(root, &getSymbolTable());
    }
    else
    {
        driverUniforms->addGraphicsDriverUniformsToShader(root, &getSymbolTable());
    }

    if (r32fImageCount > 0)
    {
        if (!RewriteR32fImages(this, root, &getSymbolTable()))
        {
            return false;
        }
    }

    if (atomicCounterCount > 0)
    {
        // ANGLEUniforms.acbBufferOffsets
        const TIntermTyped *acbBufferOffsets = driverUniforms->getAbcBufferOffsets();
        if (!RewriteAtomicCounters(this, root, &getSymbolTable(), acbBufferOffsets))
        {
            return false;
        }
    }
    else if (getShaderVersion() >= 310)
    {
        // Vulkan doesn't support Atomic Storage as a Storage Class, but we've seen
        // cases where builtins are using it even with no active atomic counters.
        // This pass simply removes those builtins in that scenario.
        if (!RemoveAtomicCounterBuiltins(this, root))
        {
            return false;
        }
    }

    if (packedShaderType != gl::ShaderType::Compute)
    {
        if (!ReplaceGLDepthRangeWithDriverUniform(this, root, driverUniforms, &getSymbolTable()))
        {
            return false;
        }

        // Search for the gl_ClipDistance/gl_CullDistance usage, if its used, we need to do some
        // replacements.
        bool useClipDistance = false;
        bool useCullDistance = false;
        for (const ShaderVariable &outputVarying : mOutputVaryings)
        {
            if (outputVarying.name == "gl_ClipDistance")
            {
                useClipDistance = true;
                break;
            }
            if (outputVarying.name == "gl_CullDistance")
            {
                useCullDistance = true;
                break;
            }
        }
        for (const ShaderVariable &inputVarying : mInputVaryings)
        {
            if (inputVarying.name == "gl_ClipDistance")
            {
                useClipDistance = true;
                break;
            }
            if (inputVarying.name == "gl_CullDistance")
            {
                useCullDistance = true;
                break;
            }
        }

        if (useClipDistance &&
            !ReplaceClipDistanceAssignments(this, root, &getSymbolTable(), getShaderType(),
                                            driverUniforms->getClipDistancesEnabled()))
        {
            return false;
        }
        if (useCullDistance &&
            !ReplaceCullDistanceAssignments(this, root, &getSymbolTable(), getShaderType()))
        {
            return false;
        }
    }

    if (gl::ShaderTypeSupportsTransformFeedback(packedShaderType))
    {
        if ((compileOptions & SH_ADD_VULKAN_XFB_EXTENSION_SUPPORT_CODE) != 0)
        {
            // Add support code for transform feedback extension.
            if (!AddXfbExtensionSupport(this, root, &getSymbolTable(), driverUniforms))
            {
                return false;
            }
        }
    }

    switch (packedShaderType)
    {
        case gl::ShaderType::Fragment:
        {
            bool usesPointCoord    = false;
            bool usesFragCoord     = false;
            bool usesSampleMaskIn  = false;
            bool usesLastFragData  = false;
            bool useSamplePosition = false;

            // Search for the gl_PointCoord usage, if its used, we need to flip the y coordinate.
            for (const ShaderVariable &inputVarying : mInputVaryings)
            {
                if (!inputVarying.isBuiltIn())
                {
                    continue;
                }

                if (inputVarying.name == "gl_SampleMaskIn")
                {
                    usesSampleMaskIn = true;
                    continue;
                }

                if (inputVarying.name == "gl_SamplePosition")
                {
                    useSamplePosition = true;
                    continue;
                }

                if (inputVarying.name == "gl_PointCoord")
                {
                    usesPointCoord = true;
                    break;
                }

                if (inputVarying.name == "gl_FragCoord")
                {
                    usesFragCoord = true;
                    break;
                }

                if (inputVarying.name == "gl_LastFragData")
                {
                    usesLastFragData = true;
                    break;
                }
            }

            if ((compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION) != 0)
            {
                if (!AddBresenhamEmulationFS(this, compileOptions, sink, root, &getSymbolTable(),
                                             specConst, driverUniforms, usesFragCoord))
                {
                    return false;
                }
            }

            bool hasGLFragColor          = false;
            bool hasGLFragData           = false;
            bool usePreRotation          = (compileOptions & SH_ADD_PRE_ROTATION) != 0;
            bool hasGLSampleMask         = false;
            bool hasGLSecondaryFragColor = false;
            bool hasGLSecondaryFragData  = false;

            for (const ShaderVariable &outputVar : mOutputVariables)
            {
                if (outputVar.name == "gl_FragColor")
                {
                    ASSERT(!hasGLFragColor);
                    hasGLFragColor = true;
                    continue;
                }
                else if (outputVar.name == "gl_FragData")
                {
                    ASSERT(!hasGLFragData);
                    hasGLFragData = true;
                    continue;
                }
                else if (outputVar.name == "gl_SampleMask")
                {
                    ASSERT(!hasGLSampleMask);
                    hasGLSampleMask = true;
                    continue;
                }
                else if (outputVar.name == "gl_SecondaryFragColorEXT")
                {
                    ASSERT(!hasGLSecondaryFragColor);
                    hasGLSecondaryFragColor = true;
                    continue;
                }
                else if (outputVar.name == "gl_SecondaryFragDataEXT")
                {
                    ASSERT(!hasGLSecondaryFragData);
                    hasGLSecondaryFragData = true;
                    continue;
                }
            }

            // Declare gl_FragColor and glFragData as webgl_FragColor and webgl_FragData
            // if it's core profile shaders and they are used.
            ASSERT(!((hasGLFragColor || hasGLSecondaryFragColor) &&
                     (hasGLFragData || hasGLSecondaryFragData)));
            if (hasGLFragColor)
            {
                sink << "layout(location = 0) out vec4 webgl_FragColor;\n";
            }
            if (hasGLFragData)
            {
                sink << "layout(location = 0) out vec4 webgl_FragData[gl_MaxDrawBuffers];\n";
            }
            if (hasGLSecondaryFragColor)
            {
                sink << "layout(location = 0, index = 1) out vec4 angle_SecondaryFragColor;\n";
            }
            if (hasGLSecondaryFragData)
            {
                sink << "layout(location = 0, index = 1) out vec4 angle_SecondaryFragData["
                     << getResources().MaxDualSourceDrawBuffers << "];\n";
            }

            if (usesPointCoord)
            {
                TIntermTyped *flipNegXY = specConst->getNegFlipXY();
                if (!flipNegXY)
                {
                    flipNegXY = driverUniforms->getNegFlipXYRef();
                }
                TIntermConstantUnion *pivot = CreateFloatNode(0.5f);
                TIntermTyped *fragRotation  = nullptr;
                if (usePreRotation)
                {
                    fragRotation = specConst->getFragRotationMatrix();
                    if (!fragRotation)
                    {
                        fragRotation = driverUniforms->getFragRotationMatrixRef();
                    }
                }
                if (!RotateAndFlipBuiltinVariable(this, root, GetMainSequence(root), flipNegXY,
                                                  &getSymbolTable(),
                                                  BuiltInVariable::gl_PointCoord(),
                                                  kFlippedPointCoordName, pivot, fragRotation))
                {
                    return false;
                }
            }

            if (useSamplePosition)
            {
                TIntermTyped *flipXY = specConst->getFlipXY();
                if (!flipXY)
                {
                    flipXY = driverUniforms->getFlipXYRef();
                }
                TIntermConstantUnion *pivot = CreateFloatNode(0.5f);
                TIntermTyped *fragRotation  = nullptr;
                if (usePreRotation)
                {
                    fragRotation = specConst->getFragRotationMatrix();
                    if (!fragRotation)
                    {
                        fragRotation = driverUniforms->getFragRotationMatrixRef();
                    }
                }
                if (!RotateAndFlipBuiltinVariable(this, root, GetMainSequence(root), flipXY,
                                                  &getSymbolTable(),
                                                  BuiltInVariable::gl_SamplePosition(),
                                                  kFlippedPointCoordName, pivot, fragRotation))
                {
                    return false;
                }
            }

            if (usesFragCoord)
            {
                if (!InsertFragCoordCorrection(this, compileOptions, root, GetMainSequence(root),
                                               &getSymbolTable(), specConst, driverUniforms))
                {
                    return false;
                }
            }

            if (usesLastFragData && !ReplaceLastFragData(this, root, &getSymbolTable(), &mUniforms))
            {
                return false;
            }

            if (!ReplaceInOutVariables(this, root, &getSymbolTable(), &mUniforms))
            {
                return false;
            }

            if (!RewriteDfdy(this, compileOptions, root, getSymbolTable(), getShaderVersion(),
                             specConst, driverUniforms))
            {
                return false;
            }

            if (!RewriteInterpolateAtOffset(this, compileOptions, root, getSymbolTable(),
                                            getShaderVersion(), specConst, driverUniforms))
            {
                return false;
            }

            if (usesSampleMaskIn && !RewriteSampleMaskIn(this, root, &getSymbolTable()))
            {
                return false;
            }

            if (hasGLSampleMask)
            {
                TIntermBinary *numSamples = driverUniforms->getNumSamplesRef();
                if (!RewriteSampleMask(this, root, &getSymbolTable(), numSamples))
                {
                    return false;
                }
            }

            {
                const TVariable *numSamplesVar =
                    static_cast<const TVariable *>(getSymbolTable().findBuiltIn(
                        ImmutableString("gl_NumSamples"), getShaderVersion()));
                TIntermBinary *numSamples = driverUniforms->getNumSamplesRef();
                if (!ReplaceVariableWithTyped(this, root, numSamplesVar, numSamples))
                {
                    return false;
                }
            }

            EmitEarlyFragmentTestsGLSL(*this, sink);
            break;
        }

        case gl::ShaderType::Vertex:
        {
            if ((compileOptions & SH_ADD_BRESENHAM_LINE_RASTER_EMULATION) != 0)
            {
                if (!AddBresenhamEmulationVS(this, root, &getSymbolTable(), specConst,
                                             driverUniforms))
                {
                    return false;
                }
            }

            if ((compileOptions & SH_ADD_VULKAN_XFB_EMULATION_SUPPORT_CODE) != 0)
            {
                // Add support code for transform feedback emulation.  Only applies to vertex shader
                // as tessellation and geometry shader transform feedback capture require
                // VK_EXT_transform_feedback.
                if (!AddXfbEmulationSupport(this, root, &getSymbolTable(), driverUniforms))
                {
                    return false;
                }
            }

            // Append depth range translation to main.
            if (!transformDepthBeforeCorrection(root, driverUniforms))
            {
                return false;
            }

            break;
        }

        case gl::ShaderType::Geometry:
        {
            int maxVertices = getGeometryShaderMaxVertices();

            // max_vertices=0 is not valid in Vulkan
            maxVertices = std::max(1, maxVertices);

            WriteGeometryShaderLayoutQualifiers(
                sink, getGeometryShaderInputPrimitiveType(), getGeometryShaderInvocations(),
                getGeometryShaderOutputPrimitiveType(), maxVertices);
            break;
        }

        case gl::ShaderType::TessControl:
        {
            WriteTessControlShaderLayoutQualifiers(sink, getTessControlShaderOutputVertices());
            break;
        }

        case gl::ShaderType::TessEvaluation:
        {
            WriteTessEvaluationShaderLayoutQualifiers(
                sink, getTessEvaluationShaderInputPrimitiveType(),
                getTessEvaluationShaderInputVertexSpacingType(),
                getTessEvaluationShaderInputOrderingType(),
                getTessEvaluationShaderInputPointType());
            break;
        }

        case gl::ShaderType::Compute:
        {
            EmitWorkGroupSizeGLSL(*this, sink);
            break;
        }

        default:
            UNREACHABLE();
            break;
    }

    specConst->outputLayoutString(sink);

    // Gather specialization constant usage bits so that we can feedback to context.
    mSpecConstUsageBits = specConst->getSpecConstUsageBits();

    if (!validateAST(root))
    {
        return false;
    }

    return true;
}

bool TranslatorVulkan::translate(TIntermBlock *root,
                                 ShCompileOptions compileOptions,
                                 PerformanceDiagnostics *perfDiagnostics)
{
    TInfoSinkBase sink;

    bool precisionEmulation = false;
    if (!emulatePrecisionIfNeeded(root, sink, &precisionEmulation, SH_SPIRV_VULKAN_OUTPUT))
        return false;

    bool enablePrecision = (compileOptions & SH_IGNORE_PRECISION_QUALIFIERS) == 0;

    TOutputVulkanGLSL outputGLSL(sink, getArrayIndexClampingStrategy(), getHashFunction(),
                                 getNameMap(), &getSymbolTable(), getShaderType(),
                                 getShaderVersion(), getOutputType(), precisionEmulation,
                                 enablePrecision, compileOptions);

    SpecConst specConst(&getSymbolTable(), compileOptions, getShaderType());

    if ((compileOptions & SH_USE_SPECIALIZATION_CONSTANT) != 0)
    {
        DriverUniform driverUniforms;
        if (!translateImpl(sink, root, compileOptions, perfDiagnostics, &specConst, &driverUniforms,
                           &outputGLSL))
        {
            return false;
        }
    }
    else
    {
        DriverUniformExtended driverUniformsExt;
        if (!translateImpl(sink, root, compileOptions, perfDiagnostics, &specConst,
                           &driverUniformsExt, &outputGLSL))
        {
            return false;
        }
    }

    // Write translated shader.
    root->traverse(&outputGLSL);

    return compileToSpirv(sink);
}

bool TranslatorVulkan::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary.
    return false;
}

bool TranslatorVulkan::compileToSpirv(const TInfoSinkBase &glsl)
{
    std::vector<uint32_t> spirvBlob;
    if (!GlslangCompileToSpirv(getResources(), getShaderType(), glsl.str(), &spirvBlob))
    {
        return false;
    }

    getInfoSink().obj.setBinary(std::move(spirvBlob));
    return true;
}
}  // namespace sh
