//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl.
//   The shaders are then fed into glslang to spit out SPIR-V (libANGLE-side).
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/TranslatorVulkan.h"

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/OutputVulkanGLSL.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/tree_ops/NameEmbeddedUniformStructs.h"
#include "compiler/translator/tree_ops/RewriteStructSamplers.h"
#include "compiler/translator/tree_util/BuiltIn_autogen.h"
#include "compiler/translator/tree_util/FindMain.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"
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
    {
    }

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
                                                emptyReplacement);
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
    {
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        // TODO(jmadill): Compound declarations.
        ASSERT(sequence.size() == 1);

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();
        bool isUniform = (type.getQualifier() == EvqUniform) && !IsOpaqueType(type.getBasicType());

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
                                                emptyReplacement);
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
            (*mSink) << HashName(name, mHashFunction, mNameMap) << ArrayString(symbol->getType());
        }
    }

  private:
    TInfoSinkBase *mSink;
    ShHashFunction64 mHashFunction;
    NameMap *mNameMap;
    bool mInDefaultUniform;
};

constexpr ImmutableString kFlippedPointCoordName = ImmutableString("flippedPointCoord");
constexpr ImmutableString kEmulatedDepthRangeParams = ImmutableString("ANGLEDepthRangeParams");

// Declares a new variable to replace gl_PointCoord with a version that is flipping the Y
// coordinate.
void FlipGLPointCoord(TIntermBlock *root,
                      const TVariable *driverUniforms,
                      TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_PointCoord"
    const TVariable *pointCoord  = BuiltInVariable::gl_PointCoord();
    TIntermSymbol *pointCoordRef = new TIntermSymbol(pointCoord);

    // Create a swizzle to "gl_PointCoord.x"
    TVector<int> swizzleOffsetX;
    swizzleOffsetX.push_back(0);
    TIntermSwizzle *pointCoordX = new TIntermSwizzle(pointCoordRef, swizzleOffsetX);

    // Create a swizzle to "gl_PointCoord.y"
    TVector<int> swizzleOffsetY;
    swizzleOffsetY.push_back(1);
    TIntermSwizzle *pointCoordY = new TIntermSwizzle(pointCoordRef, swizzleOffsetY);

    // Create a symbol reference to our new variable that will hold the modified gl_PointCoord.
    TVariable *replacementVar =
        new TVariable(symbolTable, kFlippedPointCoordName,
                      StaticType::Helpers::GetForVecMatHelper<EbtFloat, EbpMedium, EvqGlobal, 1>(2),
                      SymbolType::UserDefined);
    DeclareGlobalVariable(root, replacementVar);
    TIntermSymbol *flippedPointCoordsRef = new TIntermSymbol(replacementVar);

    const TType *constantType             = StaticType::GetBasic<EbtFloat>();

    // Create a constant "0.5"
    TConstantUnion *constantValuePointFive = new TConstantUnion();
    constantValuePointFive->setFConst(0.5f);
    TIntermConstantUnion *pointFive =
        new TIntermConstantUnion(constantValuePointFive, *constantType);

    // ANGLEUniforms.viewportScaleFactor
    TIntermSymbol *angleUniformsRef             = new TIntermSymbol(driverUniforms);
    TConstantUnion *viewportScaleFactorConstant = new TConstantUnion;
    viewportScaleFactorConstant->setIConst(1);
    TIntermConstantUnion *viewportScaleFactorIndex =
        new TIntermConstantUnion(viewportScaleFactorConstant, *StaticType::GetBasic<EbtInt>());
    TIntermBinary *viewportScaleFactorRef =
        new TIntermBinary(EOpIndexDirectInterfaceBlock, angleUniformsRef, viewportScaleFactorIndex);

    // Creates a swizzle to ANGLEUniforms.viewportScaleFactor.y
    TVector<int> viewportScaleSwizzleOffsetY;
    viewportScaleSwizzleOffsetY.push_back(1);
    TIntermSwizzle *viewportScaleY =
        new TIntermSwizzle(viewportScaleFactorRef->deepCopy(), viewportScaleSwizzleOffsetY);

    // Create the expression "(gl_PointCoord.y - 0.5) * ANGLEUniforms.viewportScaleFactor.y +
    // 0.5
    TIntermBinary *removePointFive = new TIntermBinary(EOpSub, pointCoordY, pointFive);
    TIntermBinary *inverseY        = new TIntermBinary(EOpMul, removePointFive, viewportScaleY);
    TIntermBinary *plusPointFive   = new TIntermBinary(EOpAdd, inverseY, pointFive->deepCopy());

    // Create the new vec2 using the modified Y
    TIntermSequence *sequence = new TIntermSequence();
    sequence->push_back(pointCoordX);
    sequence->push_back(plusPointFive);
    TIntermAggregate *aggregate =
        TIntermAggregate::CreateConstructor(BuiltInVariable::gl_PointCoord()->getType(), sequence);

    // Use this new variable instead of gl_PointCoord everywhere.
    ReplaceVariable(root, pointCoord, replacementVar);

    // Assign this new value to flippedPointCoord
    TIntermBinary *assignment = new TIntermBinary(EOpInitialize, flippedPointCoordsRef, aggregate);

    // Add this assigment at the beginning of the main function
    TIntermFunctionDefinition *main = FindMain(root);
    TIntermSequence *mainSequence   = main->getBody()->getSequence();
    mainSequence->insert(mainSequence->begin(), assignment);
}

// Declares a new variable to replace gl_DepthRange, its values are fed from a driver uniform.
void ReplaceGLDepthRangeWithDriverUniform(TIntermBlock *root,
                                          const TVariable *driverUniforms,
                                          TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_DepthRange"
    const TVariable *depthRangeVar = static_cast<const TVariable *>(
        symbolTable->findBuiltIn(ImmutableString("gl_DepthRange"), 0));

    // ANGLEUniforms.depthRange
    TIntermSymbol *angleUniformsRef    = new TIntermSymbol(driverUniforms);
    TConstantUnion *depthRangeConstant = new TConstantUnion;
    depthRangeConstant->setIConst(2);
    TIntermConstantUnion *depthRangeIndex =
        new TIntermConstantUnion(depthRangeConstant, *StaticType::GetBasic<EbtInt>());
    TIntermBinary *angleEmulatedDepthRangeRef =
        new TIntermBinary(EOpIndexDirectInterfaceBlock, angleUniformsRef, depthRangeIndex);

    // Use this variable instead of gl_DepthRange everywhere.
    ReplaceVariableWithTyped(root, depthRangeVar, angleEmulatedDepthRangeRef);
}

// This operation performs the viewport depth translation needed by Vulkan. In GL the viewport
// transformation is slightly different - see the GL 2.0 spec section "2.12.1 Controlling the
// Viewport". In Vulkan the corresponding spec section is currently "23.4. Coordinate
// Transformations".
// The equations reduce to an expression:
//
//     z_vk = w_gl * (0.5 * z_gl + 0.5)
//
// where z_vk is the depth output of a Vulkan vertex shader and z_gl is the same for GL.
void AppendVertexShaderDepthCorrectionToMain(TIntermBlock *root, TSymbolTable *symbolTable)
{
    // Create a symbol reference to "gl_Position"
    const TVariable *position  = BuiltInVariable::gl_Position();
    TIntermSymbol *positionRef = new TIntermSymbol(position);

    // Create a swizzle to "gl_Position.z"
    TVector<int> swizzleOffsetZ;
    swizzleOffsetZ.push_back(2);
    TIntermSwizzle *positionZ = new TIntermSwizzle(positionRef, swizzleOffsetZ);

    // Create a constant "0.5"
    const TType *constantType     = StaticType::GetBasic<TBasicType::EbtFloat>();
    TConstantUnion *constantValue = new TConstantUnion();
    constantValue->setFConst(0.5f);
    TIntermConstantUnion *oneHalf = new TIntermConstantUnion(constantValue, *constantType);

    // Create the expression "gl_Position.z * 0.5 + 0.5"
    TIntermBinary *halfZ         = new TIntermBinary(TOperator::EOpMul, positionZ, oneHalf);
    TIntermBinary *halfZPlusHalf = new TIntermBinary(TOperator::EOpAdd, halfZ, oneHalf->deepCopy());

    // Create a swizzle to "gl_Position.w"
    TVector<int> swizzleOffsetW;
    swizzleOffsetW.push_back(3);
    TIntermSwizzle *positionW = new TIntermSwizzle(positionRef->deepCopy(), swizzleOffsetW);

    // Create the expression "gl_Position.w * (gl_Position.z * 0.5 + 0.5)"
    TIntermBinary *vulkanZ = new TIntermBinary(TOperator::EOpMul, positionW, halfZPlusHalf);

    // Create the assignment "gl_Position.z = gl_Position.w * (gl_Position.z * 0.5 + 0.5)"
    TIntermTyped *positionZLHS = positionZ->deepCopy();
    TIntermBinary *assignment  = new TIntermBinary(TOperator::EOpAssign, positionZLHS, vulkanZ);

    // Append the assignment as a statement at the end of the shader.
    RunAtTheEndOfShader(root, assignment, symbolTable);
}

// The AddDriverUniformsToShader operation adds an internal uniform block to a shader. The driver
// block is used to implement Vulkan-specific features and workarounds. Returns the driver uniforms
// variable.
const TVariable *AddDriverUniformsToShader(TIntermBlock *root, TSymbolTable *symbolTable)
{
    // This field list mirrors the structure of ContextVk::DriverUniforms.
    TFieldList *driverFieldList = new TFieldList;

    // Add a vec4 field "viewport" to the driver uniform fields.
    TType *driverViewportType  = new TType(EbtFloat, 4);
    TField *driverViewportSize = new TField(driverViewportType, ImmutableString("viewport"),
                                            TSourceLoc(), SymbolType::AngleInternal);
    driverFieldList->push_back(driverViewportSize);

    // Add a vec4 field "viewportScaleFactor" to the driver uniform fields.
    TType *driverViewportScaleFactorType = new TType(EbtFloat, 4);
    TField *driverViewportScaleFactorSize =
        new TField(driverViewportScaleFactorType, ImmutableString("viewportScaleFactor"),
                   TSourceLoc(), SymbolType::AngleInternal);
    driverFieldList->push_back(driverViewportScaleFactorSize);

    // Add a new struct field "depthRange" to the driver uniform fields.
    const TSourceLoc zeroSourceLoc     = {0, 0, 0, 0};
    TFieldList *depthRangeParamsFields = new TFieldList();
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("near"), zeroSourceLoc,
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("far"), zeroSourceLoc,
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("diff"), zeroSourceLoc,
                                                 SymbolType::AngleInternal));
    depthRangeParamsFields->push_back(new TField(new TType(EbtFloat, EbpHigh, EvqGlobal, 1, 1),
                                                 ImmutableString("dummyPacker"), zeroSourceLoc,
                                                 SymbolType::AngleInternal));
    TStructure *emulatedDepthRangeParams = new TStructure(
        symbolTable, kEmulatedDepthRangeParams, depthRangeParamsFields, SymbolType::AngleInternal);
    TType *emulatedDepthRangeType = new TType(emulatedDepthRangeParams, false);
    TVariable *depthRangeVar =
        new TVariable(symbolTable->nextUniqueId(), kEmptyImmutableString, SymbolType::Empty,
                      TExtension::UNDEFINED, emulatedDepthRangeType);

    DeclareGlobalVariable(root, depthRangeVar);

    TField *driverDepthRangeSize = new TField(emulatedDepthRangeType, ImmutableString("depthRange"),
                                              TSourceLoc(), SymbolType::AngleInternal);
    driverFieldList->push_back(driverDepthRangeSize);

    // Define a driver uniform block "ANGLEUniformBlock".
    TLayoutQualifier driverLayoutQualifier = TLayoutQualifier::Create();
    TInterfaceBlock *interfaceBlock =
        new TInterfaceBlock(symbolTable, ImmutableString("ANGLEUniformBlock"), driverFieldList,
                            driverLayoutQualifier, SymbolType::AngleInternal);

    // Make the inteface block into a declaration. Use instance name "ANGLEUniforms".
    TType *interfaceBlockType = new TType(interfaceBlock, EvqUniform, driverLayoutQualifier);
    TIntermDeclaration *driverUniformsDecl = new TIntermDeclaration;
    TVariable *driverUniformsVar = new TVariable(symbolTable, ImmutableString("ANGLEUniforms"),
                                                 interfaceBlockType, SymbolType::AngleInternal);
    TIntermSymbol *driverUniformsDeclarator = new TIntermSymbol(driverUniformsVar);
    driverUniformsDecl->appendDeclarator(driverUniformsDeclarator);

    // Insert the declarations before Main.
    TIntermSequence *insertSequence = new TIntermSequence;
    insertSequence->push_back(driverUniformsDecl);

    size_t mainIndex = FindMainIndex(root);
    root->insertChildNodes(mainIndex, *insertSequence);

    return driverUniformsVar;
}
}  // anonymous namespace

TranslatorVulkan::TranslatorVulkan(sh::GLenum type, ShShaderSpec spec)
    : TCompiler(type, spec, SH_GLSL_450_CORE_OUTPUT)
{
}

void TranslatorVulkan::translate(TIntermBlock *root,
                                 ShCompileOptions compileOptions,
                                 PerformanceDiagnostics * /*perfDiagnostics*/)
{
    TInfoSinkBase &sink = getInfoSink().obj;
    TOutputVulkanGLSL outputGLSL(sink, getArrayIndexClampingStrategy(), getHashFunction(),
                                 getNameMap(), &getSymbolTable(), getShaderType(),
                                 getShaderVersion(), getOutputType(), compileOptions);

    sink << "#version 450 core\n";

    // Write out default uniforms into a uniform block assigned to a specific set/binding.
    int defaultUniformCount        = 0;
    int structTypesUsedForUniforms = 0;
    for (const auto &uniform : getUniforms())
    {
        if (!uniform.isBuiltIn() && uniform.staticUse && !gl::IsOpaqueType(uniform.type))
        {
            ++defaultUniformCount;
        }

        if (uniform.isStruct())
        {
            ++structTypesUsedForUniforms;
        }
    }

    // TODO(lucferron): Refactor this function to do less tree traversals.
    // http://anglebug.com/2461
    if (structTypesUsedForUniforms > 0)
    {
        NameEmbeddedStructUniforms(root, &getSymbolTable());

        defaultUniformCount -= RewriteStructSamplers(root, &getSymbolTable());

        // We must declare the struct types before using them.
        DeclareStructTypesTraverser structTypesTraverser(&outputGLSL);
        root->traverse(&structTypesTraverser);
        structTypesTraverser.updateTree();
    }

    if (defaultUniformCount > 0)
    {
        sink << "\nlayout(@@ DEFAULT-UNIFORMS-SET-BINDING @@) uniform defaultUniforms\n{\n";

        DeclareDefaultUniformsTraverser defaultTraverser(&sink, getHashFunction(), &getNameMap());
        root->traverse(&defaultTraverser);
        defaultTraverser.updateTree();

        sink << "};\n";
    }

    const TVariable *driverUniformsVariable = AddDriverUniformsToShader(root, &getSymbolTable());

    ReplaceGLDepthRangeWithDriverUniform(root, driverUniformsVariable, &getSymbolTable());

    // Declare gl_FragColor and glFragData as webgl_FragColor and webgl_FragData
    // if it's core profile shaders and they are used.
    if (getShaderType() == GL_FRAGMENT_SHADER)
    {
        bool hasGLFragColor = false;
        bool hasGLFragData  = false;

        for (const OutputVariable &outputVar : outputVariables)
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
        }
        ASSERT(!(hasGLFragColor && hasGLFragData));
        if (hasGLFragColor)
        {
            sink << "layout(location = 0) out vec4 webgl_FragColor;\n";
        }
        if (hasGLFragData)
        {
            sink << "layout(location = 0) out vec4 webgl_FragData[gl_MaxDrawBuffers];\n";
        }

        // Search for the gl_PointCoord usage, if its used, we need to flip the y coordinate.
        for (const Varying &inputVarying : inputVaryings)
        {
            if (!inputVarying.isBuiltIn())
            {
                continue;
            }

            if (inputVarying.name == "gl_PointCoord")
            {
                FlipGLPointCoord(root, driverUniformsVariable, &getSymbolTable());
                break;
            }
        }
    }
    else
    {
        ASSERT(getShaderType() == GL_VERTEX_SHADER);

        // Append depth range translation to main.
        AppendVertexShaderDepthCorrectionToMain(root, &getSymbolTable());
    }

    // Write translated shader.
    root->traverse(&outputGLSL);
}

bool TranslatorVulkan::shouldFlattenPragmaStdglInvariantAll()
{
    // Not necessary.
    return false;
}

}  // namespace sh
