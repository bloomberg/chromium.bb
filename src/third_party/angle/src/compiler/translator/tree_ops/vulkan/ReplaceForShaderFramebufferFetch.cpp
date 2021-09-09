//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReplaceForShaderFramebufferFetch.h: Find any references to gl_LastFragData, and replace it with
// ANGLELastFragData.
//

#include "compiler/translator/tree_ops/vulkan/ReplaceForShaderFramebufferFetch.h"

#include "common/bitset_utils.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/RunAtTheBeginningOfShader.h"
#include "compiler/translator/tree_util/SpecializationConstant.h"
#include "compiler/translator/util.h"

namespace sh
{
namespace
{

using InputAttachmentIdxSet = angle::BitSet<32>;
using MapForReplacement     = const std::map<const TVariable *, const TIntermTyped *>;

constexpr unsigned int kInputAttachmentZero = 0;
constexpr unsigned int kArraySizeZero       = 0;

enum class InputType
{
    SubpassInput = 0,
    SubpassInputMS,
    ISubpassInput,
    ISubpassInputMS,
    USubpassInput,
    USubpassInputMS,

    InvalidEnum,
    EnumCount = InvalidEnum,
};

class InputAttachmentReferenceTraverser : public TIntermTraverser
{
  public:
    InputAttachmentReferenceTraverser(std::map<unsigned int, TIntermSymbol *> *declaredSymOut,
                                      unsigned int *maxInputAttachmentIndex,
                                      InputAttachmentIdxSet *constIndicesOut,
                                      bool *usedNonConstIndex)
        : TIntermTraverser(true, false, false),
          mDeclaredSym(declaredSymOut),
          mMaxInputAttachmentIndex(maxInputAttachmentIndex),
          mConstInputAttachmentIndices(constIndicesOut),
          mUsedNonConstIndex(usedNonConstIndex)
    {
        mDeclaredSym->clear();
        *mMaxInputAttachmentIndex = 0;
        mConstInputAttachmentIndices->reset();
        *mUsedNonConstIndex = false;
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;

  private:
    void setInputAttachmentIndex(unsigned int index);

    std::map<unsigned int, TIntermSymbol *> *mDeclaredSym;
    unsigned int *mMaxInputAttachmentIndex;
    InputAttachmentIdxSet *mConstInputAttachmentIndices;
    bool *mUsedNonConstIndex;
};

class ReplaceVariableTraverser : public TIntermTraverser
{
  public:
    ReplaceVariableTraverser(
        const std::map<const TVariable *, const TIntermTyped *> &replacementMap)
        : TIntermTraverser(true, false, false), mReplacementMap(replacementMap)
    {}

    ReplaceVariableTraverser(const TVariable *toBeReplaced, const TIntermTyped *replacement)
        : TIntermTraverser(true, false, false), mReplacementMap({{toBeReplaced, replacement}})
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;
    void visitSymbol(TIntermSymbol *node) override;

  private:
    const std::map<const TVariable *, const TIntermTyped *> mReplacementMap;
};

void InputAttachmentReferenceTraverser::setInputAttachmentIndex(unsigned int inputAttachmentIdx)
{
    ASSERT(inputAttachmentIdx < mConstInputAttachmentIndices->size());
    mConstInputAttachmentIndices->set(inputAttachmentIdx);
    *mMaxInputAttachmentIndex = std::max(*mMaxInputAttachmentIndex, inputAttachmentIdx);
}

bool InputAttachmentReferenceTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    const TIntermSequence &sequence = *(node->getSequence());

    if (sequence.size() != 1)
    {
        return true;
    }

    TIntermTyped *variable = sequence.front()->getAsTyped();
    TIntermSymbol *symbol  = variable->getAsSymbolNode();
    if (symbol == nullptr)
    {
        return true;
    }

    if (symbol->getType().getQualifier() == EvqFragmentInOut)
    {
        unsigned int inputAttachmentIdx = symbol->getType().getLayoutQualifier().location;

        if (symbol->getType().isArray())
        {
            for (unsigned int index = 0; index < symbol->getType().getOutermostArraySize(); index++)
            {
                unsigned int realInputAttachmentIdx = inputAttachmentIdx + index;
                setInputAttachmentIndex(realInputAttachmentIdx);
            }
        }
        else
        {
            setInputAttachmentIndex(inputAttachmentIdx);
        }

        mDeclaredSym->emplace(inputAttachmentIdx, symbol);
    }

    return true;
}

bool InputAttachmentReferenceTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    TOperator op = node->getOp();
    if (op != EOpIndexDirect && op != EOpIndexIndirect)
    {
        return true;
    }

    TIntermSymbol *left = node->getLeft()->getAsSymbolNode();
    if (!left)
    {
        return true;
    }
    else if (left->getName() != "gl_LastFragData")
    {
        return true;
    }

    const TConstantUnion *constIdx = node->getRight()->getConstantValue();
    if (!constIdx)
    {
        // If the shader code uses gl_LastFragData with a non-const index, the input attachment
        // variable should be created as the maximum number. So, the previous redeclared
        // variable will be reset.
        mDeclaredSym->clear();

        *mUsedNonConstIndex = true;
        mDeclaredSym->emplace(0, left);
        return true;
    }
    else
    {
        unsigned int idx = 0;
        switch (constIdx->getType())
        {
            case EbtInt:
                idx = constIdx->getIConst();
                break;
            case EbtUInt:
                idx = constIdx->getUConst();
                break;
            case EbtFloat:
                idx = static_cast<unsigned int>(constIdx->getFConst());
                break;
            case EbtBool:
                idx = constIdx->getBConst() ? 1 : 0;
                break;
            default:
                UNREACHABLE();
                break;
        }
        ASSERT(idx < mConstInputAttachmentIndices->size());
        mConstInputAttachmentIndices->set(idx);

        *mMaxInputAttachmentIndex = std::max(*mMaxInputAttachmentIndex, idx);
        mDeclaredSym->emplace(idx, left);
    }

    return true;
}

bool ReplaceVariableTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    const TIntermSequence &sequence = *(node->getSequence());
    if (sequence.size() != 1)
    {
        return true;
    }

    TIntermTyped *nodeType = sequence.front()->getAsTyped();
    TIntermSymbol *symbol  = nodeType->getAsSymbolNode();
    if (symbol == nullptr)
    {
        return true;
    }

    const TVariable *variable = &symbol->variable();
    if (mReplacementMap.find(variable) != mReplacementMap.end())
    {
        TIntermSequence emptyReplacement;
        mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                        std::move(emptyReplacement));

        return true;
    }

    return true;
}

void ReplaceVariableTraverser::visitSymbol(TIntermSymbol *node)
{
    const TVariable *variable = &node->variable();
    if (mReplacementMap.find(variable) != mReplacementMap.end())
    {
        queueReplacement(mReplacementMap.at(variable)->deepCopy(), OriginalNode::IS_DROPPED);
    }
}

InputType GetInputTypeOfSubpassInput(const TBasicType &basicType)
{
    switch (basicType)
    {
        case TBasicType::EbtSubpassInput:
            return InputType::SubpassInput;
        case TBasicType::EbtSubpassInputMS:
            return InputType::SubpassInputMS;
        case TBasicType::EbtISubpassInput:
            return InputType::ISubpassInput;
        case TBasicType::EbtISubpassInputMS:
            return InputType::ISubpassInputMS;
        case TBasicType::EbtUSubpassInput:
            return InputType::USubpassInput;
        case TBasicType::EbtUSubpassInputMS:
            return InputType::USubpassInputMS;
        default:
            UNREACHABLE();
            return InputType::InvalidEnum;
    }
}

TBasicType GetBasicTypeOfSubpassInput(const InputType &inputType)
{
    switch (inputType)
    {
        case InputType::SubpassInput:
            return EbtSubpassInput;
        case InputType::SubpassInputMS:
            return EbtSubpassInputMS;
        case InputType::ISubpassInput:
            return EbtISubpassInput;
        case InputType::ISubpassInputMS:
            return EbtISubpassInputMS;
        case InputType::USubpassInput:
            return EbtUSubpassInput;
        case InputType::USubpassInputMS:
            return EbtUSubpassInputMS;
        default:
            UNREACHABLE();
            return TBasicType::EbtVoid;
    }
}

TBasicType GetBasicTypeForSubpassInput(const TBasicType &inputType)
{
    switch (inputType)
    {
        case EbtFloat:
            return EbtSubpassInput;
        case EbtInt:
            return EbtISubpassInput;
        case EbtUInt:
            return EbtUSubpassInput;
        default:
            UNREACHABLE();
            return EbtVoid;
    }
}

TBasicType GetBasicTypeForSubpassInput(const TIntermSymbol *originSymbol)
{
    if (originSymbol->getName().beginsWith("gl_LastFragData"))
    {
        return GetBasicTypeForSubpassInput(EbtFloat);
    }

    return GetBasicTypeForSubpassInput(originSymbol->getBasicType());
}

ImmutableString GetTypeNameOfSubpassInput(const InputType &inputType)
{
    switch (inputType)
    {
        case InputType::SubpassInput:
            return ImmutableString("subpassInput");
        case InputType::SubpassInputMS:
            return ImmutableString("subpassInputMS");
        case InputType::ISubpassInput:
            return ImmutableString("isubpassInput");
        case InputType::ISubpassInputMS:
            return ImmutableString("isubpassInputMS");
        case InputType::USubpassInput:
            return ImmutableString("usubpassInput");
        case InputType::USubpassInputMS:
            return ImmutableString("usubpassInputMS");
        default:
            UNREACHABLE();
            return kEmptyImmutableString;
    }
}

ImmutableString GetFunctionNameOfSubpassLoad(const InputType &inputType)
{
    switch (inputType)
    {
        case InputType::SubpassInput:
        case InputType::ISubpassInput:
        case InputType::USubpassInput:
            return ImmutableString("subpassLoad");
        case InputType::SubpassInputMS:
        case InputType::ISubpassInputMS:
        case InputType::USubpassInputMS:
            return ImmutableString("subpassLoadMS");
        default:
            UNREACHABLE();
            return kEmptyImmutableString;
    }
}

TIntermAggregate *CreateSubpassLoadFuncCall(TSymbolTable *symbolTable,
                                            std::map<InputType, TFunction *> *functionMap,
                                            const InputType &inputType,
                                            TIntermSequence *arguments)
{
    TBasicType subpassInputType = GetBasicTypeOfSubpassInput(inputType);
    ASSERT(subpassInputType != TBasicType::EbtVoid);

    TFunction **currentFunc = &(*functionMap)[inputType];
    if (*currentFunc == nullptr)
    {
        TType *inputAttachmentType = new TType(subpassInputType, EbpUndefined, EvqUniform, 1);
        *currentFunc = new TFunction(symbolTable, GetFunctionNameOfSubpassLoad(inputType),
                                     SymbolType::AngleInternal,
                                     new TType(EbtFloat, EbpUndefined, EvqGlobal, 4, 1), true);
        (*currentFunc)
            ->addParameter(new TVariable(symbolTable, GetTypeNameOfSubpassInput(inputType),
                                         inputAttachmentType, SymbolType::AngleInternal));
    }

    return TIntermAggregate::CreateFunctionCall(**currentFunc, arguments);
}

class ReplaceSubpassInputUtils
{
  public:
    ReplaceSubpassInputUtils(TCompiler *compiler,
                             TSymbolTable *symbolTable,
                             TIntermBlock *root,
                             std::vector<ShaderVariable> *uniforms,
                             const bool usedNonConstIndex,
                             const InputAttachmentIdxSet &constIndices,
                             const std::map<unsigned int, TIntermSymbol *> &declaredVarVec)
        : mCompiler(compiler),
          mSymbolTable(symbolTable),
          mRoot(root),
          mUniforms(uniforms),
          mUsedNonConstIndex(usedNonConstIndex),
          mConstIndices(constIndices),
          mDeclaredVarVec(declaredVarVec)
    {
        mDeclareVariables.clear();
        mInputAttachmentArrayIdSeq = 0;
        mInputAttachmentVarList.clear();
        mDataLoadVarList.clear();
        mFunctionMap.clear();
    }
    virtual ~ReplaceSubpassInputUtils() = default;

    virtual bool declareSubpassInputVariables() = 0;
    void declareVariablesForFetch(const unsigned int inputAttachmentIndex,
                                  const TVariable *dataLoadVar)
    {
        mDataLoadVarList[inputAttachmentIndex] = dataLoadVar;

        TIntermDeclaration *dataLoadVarDecl = new TIntermDeclaration;
        TIntermSymbol *dataLoadVarDeclarator =
            new TIntermSymbol(mDataLoadVarList[inputAttachmentIndex]);
        dataLoadVarDecl->appendDeclarator(dataLoadVarDeclarator);
        mDeclareVariables.push_back(dataLoadVarDecl);
    }

    virtual bool loadInputAttachmentData() = 0;

    void submitNewDeclaration()
    {
        for (unsigned int index = 0; index < mDeclareVariables.size(); index++)
        {
            mRoot->insertStatement(index, mDeclareVariables[index]);
        }

        mDeclareVariables.clear();
    }

  protected:
    bool declareSubpassInputVariableImpl(const TIntermSymbol *declaredVarSym,
                                         const unsigned int inputAttachmentIndex);
    void addInputAttachmentUniform(const unsigned int inputAttachmentIndex);

    TIntermNode *assignSubpassLoad(TIntermTyped *resultVar,
                                   TIntermTyped *inputAttachmentSymbol,
                                   const int targetVecSize);
    TIntermNode *loadInputAttachmentDataImpl(const size_t arraySize,
                                             const unsigned int inputAttachmentIndex,
                                             const TVariable *loadInputAttachmentDataVar);

    ImmutableString getInputAttachmentName(unsigned int index);
    ImmutableString getInputAttachmentArrayName();

    TCompiler *mCompiler;
    TSymbolTable *mSymbolTable;
    TIntermBlock *mRoot;
    std::vector<ShaderVariable> *mUniforms;
    const bool mUsedNonConstIndex;
    const InputAttachmentIdxSet mConstIndices;
    const std::map<unsigned int, TIntermSymbol *> mDeclaredVarVec;

    TIntermSequence mDeclareVariables;
    unsigned int mInputAttachmentArrayIdSeq;
    std::map<InputType, TFunction *> mFunctionMap;
    std::map<unsigned int, TVariable *> mInputAttachmentVarList;
    std::map<unsigned int, const TVariable *> mDataLoadVarList;
};

ImmutableString ReplaceSubpassInputUtils::getInputAttachmentArrayName()
{
    constexpr ImmutableString suffix("Array");
    std::stringstream nameStream = sh::InitializeStream<std::stringstream>();
    nameStream << sh::vk::kInputAttachmentName << suffix << mInputAttachmentArrayIdSeq++;
    return ImmutableString(nameStream.str());
}

ImmutableString ReplaceSubpassInputUtils::getInputAttachmentName(unsigned int index)
{
    std::stringstream nameStream = sh::InitializeStream<std::stringstream>();
    nameStream << sh::vk::kInputAttachmentName << index;
    return ImmutableString(nameStream.str());
}

bool ReplaceSubpassInputUtils::declareSubpassInputVariableImpl(
    const TIntermSymbol *declaredVarSym,
    const unsigned int inputAttachmentIndex)
{
    TBasicType subpassInputType = GetBasicTypeForSubpassInput(declaredVarSym);
    if (subpassInputType == EbtVoid)
    {
        return false;
    }

    TType *inputAttachmentType = new TType(subpassInputType, EbpUndefined, EvqUniform, 1);
    TLayoutQualifier inputAttachmentQualifier     = inputAttachmentType->getLayoutQualifier();
    inputAttachmentQualifier.inputAttachmentIndex = inputAttachmentIndex;
    inputAttachmentType->setLayoutQualifier(inputAttachmentQualifier);

    mInputAttachmentVarList[inputAttachmentIndex] =
        new TVariable(mSymbolTable, getInputAttachmentName(inputAttachmentIndex),
                      inputAttachmentType, SymbolType::AngleInternal);
    TIntermSymbol *inputAttachmentDeclarator =
        new TIntermSymbol(mInputAttachmentVarList[inputAttachmentIndex]);

    TIntermDeclaration *inputAttachmentDecl = new TIntermDeclaration;
    inputAttachmentDecl->appendDeclarator(inputAttachmentDeclarator);

    mDeclareVariables.push_back(inputAttachmentDecl);

    return true;
}

void ReplaceSubpassInputUtils::addInputAttachmentUniform(const unsigned int inputAttachmentIndex)
{
    const TVariable *inputAttachmentVar = mInputAttachmentVarList[inputAttachmentIndex];

    ShaderVariable inputAttachmentUniform;
    inputAttachmentUniform.active    = true;
    inputAttachmentUniform.staticUse = true;
    inputAttachmentUniform.name.assign(inputAttachmentVar->name().data(),
                                       inputAttachmentVar->name().length());
    inputAttachmentUniform.mappedName.assign(inputAttachmentUniform.name);
    inputAttachmentUniform.isFragmentInOut = true;
    inputAttachmentUniform.location =
        inputAttachmentVar->getType().getLayoutQualifier().inputAttachmentIndex;
    mUniforms->push_back(inputAttachmentUniform);
}

TIntermNode *ReplaceSubpassInputUtils::assignSubpassLoad(TIntermTyped *resultVar,
                                                         TIntermTyped *inputAttachmentSymbol,
                                                         const int targetVecSize)
{
    TIntermSequence *subpassArguments = new TIntermSequence();
    subpassArguments->push_back(inputAttachmentSymbol);

    TIntermAggregate *subpassLoadFuncCall = CreateSubpassLoadFuncCall(
        mSymbolTable, &mFunctionMap,
        GetInputTypeOfSubpassInput(inputAttachmentSymbol->getBasicType()), subpassArguments);

    TVector<int> fieldOffsets(targetVecSize);
    for (int i = 0; i < targetVecSize; i++)
    {
        fieldOffsets[i] = i;
    }

    TIntermTyped *right = new TIntermSwizzle(subpassLoadFuncCall, fieldOffsets);

    return new TIntermBinary(EOpAssign, resultVar, right);
}

TIntermNode *ReplaceSubpassInputUtils::loadInputAttachmentDataImpl(
    const size_t arraySize,
    const unsigned int inputAttachmentIndex,
    const TVariable *loadInputAttachmentDataVar)
{
    TIntermNode *retExpression = nullptr;

    TIntermSymbol *loadInputAttachmentDataSymbol = new TIntermSymbol(loadInputAttachmentDataVar);

    TIntermTyped *left = nullptr;

    if (arraySize > 0)
    {
        TIntermBlock *blockNode = new TIntermBlock();

        for (uint32_t index = 0; index < arraySize; index++)
        {
            uint32_t attachmentIndex = inputAttachmentIndex + index;

            left = new TIntermBinary(EOpIndexDirect, loadInputAttachmentDataSymbol->deepCopy(),
                                     CreateIndexNode(index));

            blockNode->appendStatement(
                assignSubpassLoad(left, new TIntermSymbol(mInputAttachmentVarList[attachmentIndex]),
                                  left->getNominalSize()));
        }

        retExpression = blockNode;
    }
    else
    {
        if (loadInputAttachmentDataSymbol->isArray())
        {
            left = new TIntermBinary(EOpIndexDirect, loadInputAttachmentDataSymbol->deepCopy(),
                                     CreateIndexNode(inputAttachmentIndex));
        }
        else
        {
            left = loadInputAttachmentDataSymbol->deepCopy();
        }

        retExpression = assignSubpassLoad(
            left, new TIntermSymbol(mInputAttachmentVarList[inputAttachmentIndex]),
            left->getNominalSize());
    }

    return retExpression;
}

class ReplaceGlLastFragDataUtils : public ReplaceSubpassInputUtils
{
  public:
    ReplaceGlLastFragDataUtils(TCompiler *compiler,
                               TSymbolTable *symbolTable,
                               TIntermBlock *root,
                               std::vector<ShaderVariable> *uniforms,
                               const bool usedNonConstIndex,
                               const InputAttachmentIdxSet &constIndices,
                               const std::map<unsigned int, TIntermSymbol *> &declaredVarVec)
        : ReplaceSubpassInputUtils(compiler,
                                   symbolTable,
                                   root,
                                   uniforms,
                                   usedNonConstIndex,
                                   constIndices,
                                   declaredVarVec)
    {}

    bool declareSubpassInputVariables() override;
    bool loadInputAttachmentData() override;
};

bool ReplaceGlLastFragDataUtils::declareSubpassInputVariables()
{
    for (auto declaredVar : mDeclaredVarVec)
    {
        const unsigned int inputAttachmentIndex = declaredVar.first;
        if (mConstIndices.test(inputAttachmentIndex))
        {
            if (!declareSubpassInputVariableImpl(declaredVar.second, inputAttachmentIndex))
            {
                return false;
            }

            addInputAttachmentUniform(inputAttachmentIndex);
        }
    }

    return true;
}

bool ReplaceGlLastFragDataUtils::loadInputAttachmentData()
{
    TIntermNode *loadInputAttachmentBlock = new TIntermBlock();

    for (auto declaredVar : mDeclaredVarVec)
    {
        const unsigned int inputAttachmentIndex = declaredVar.first;
        if (mConstIndices.test(inputAttachmentIndex))
        {
            loadInputAttachmentBlock->getAsBlock()->appendStatement(loadInputAttachmentDataImpl(
                kArraySizeZero, inputAttachmentIndex, mDataLoadVarList[kArraySizeZero]));
        }
    }

    ASSERT(loadInputAttachmentBlock->getChildCount() > 0);
    if (!RunAtTheBeginningOfShader(mCompiler, mRoot, loadInputAttachmentBlock))
    {
        return false;
    }

    return true;
}

class ReplaceInOutUtils : public ReplaceSubpassInputUtils
{
  public:
    ReplaceInOutUtils(TCompiler *compiler,
                      TSymbolTable *symbolTable,
                      TIntermBlock *root,
                      std::vector<ShaderVariable> *uniforms,
                      const bool usedNonConstIndex,
                      const InputAttachmentIdxSet &constIndices,
                      const std::map<unsigned int, TIntermSymbol *> &declaredVarVec)
        : ReplaceSubpassInputUtils(compiler,
                                   symbolTable,
                                   root,
                                   uniforms,
                                   usedNonConstIndex,
                                   constIndices,
                                   declaredVarVec)
    {}

    bool declareSubpassInputVariables() override;
    bool loadInputAttachmentData() override;
};

bool ReplaceInOutUtils::declareSubpassInputVariables()
{
    for (auto declaredVar : mDeclaredVarVec)
    {
        const unsigned int inputAttachmentIndex = declaredVar.first;
        const unsigned int arraySize =
            (declaredVar.second->isArray() ? declaredVar.second->getOutermostArraySize() : 1);

        for (unsigned int arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
        {
            unsigned int attachmentIndex = inputAttachmentIndex + arrayIndex;

            if (!declareSubpassInputVariableImpl(declaredVar.second, attachmentIndex))
            {
                return false;
            }

            addInputAttachmentUniform(attachmentIndex);
        }
    }

    return true;
}

bool ReplaceInOutUtils::loadInputAttachmentData()
{
    TIntermBlock *loadInputAttachmentBlock = new TIntermBlock();

    for (auto declaredVar : mDeclaredVarVec)
    {
        const unsigned int inputAttachmentIndex = declaredVar.first;
        size_t arraySize =
            (declaredVar.second->isArray() ? declaredVar.second->getOutermostArraySize() : 0);
        loadInputAttachmentBlock->appendStatement(loadInputAttachmentDataImpl(
            arraySize, inputAttachmentIndex, mDataLoadVarList[inputAttachmentIndex]));
    }

    ASSERT(loadInputAttachmentBlock->getChildCount() > 0);
    if (!RunAtTheBeginningOfShader(mCompiler, mRoot, loadInputAttachmentBlock))
    {
        return false;
    }

    return true;
}

}  // anonymous namespace

ANGLE_NO_DISCARD bool ReplaceLastFragData(TCompiler *compiler,
                                          TIntermBlock *root,
                                          TSymbolTable *symbolTable,
                                          std::vector<ShaderVariable> *uniforms)
{
    // Common variables
    InputAttachmentIdxSet constIndices;
    std::map<unsigned int, TIntermSymbol *> glLastFragDataUsageMap;
    unsigned int maxInputAttachmentIndex = 0;
    bool usedNonConstIndex               = false;

    // Get informations for gl_LastFragData
    InputAttachmentReferenceTraverser informationTraverser(
        &glLastFragDataUsageMap, &maxInputAttachmentIndex, &constIndices, &usedNonConstIndex);
    root->traverse(&informationTraverser);
    if (constIndices.none() && !usedNonConstIndex)
    {
        // No references of gl_LastFragData
        return true;
    }

    // Declare subpassInput uniform variables
    ReplaceGlLastFragDataUtils replaceSubpassInputUtils(compiler, symbolTable, root, uniforms,
                                                        usedNonConstIndex, constIndices,
                                                        glLastFragDataUsageMap);
    if (!replaceSubpassInputUtils.declareSubpassInputVariables())
    {
        return false;
    }

    // Declare the variables which store the result of subpassLoad function
    const TVariable *glLastFragDataVar = nullptr;
    if (glLastFragDataUsageMap.size() > 0)
    {
        glLastFragDataVar = &glLastFragDataUsageMap.begin()->second->variable();
    }
    else
    {
        glLastFragDataVar = static_cast<const TVariable *>(
            symbolTable->findBuiltIn(ImmutableString("gl_LastFragData"), 100));
    }
    if (!glLastFragDataVar)
    {
        return false;
    }

    const TBasicType loadVarBasicType = glLastFragDataVar->getType().getBasicType();
    const TPrecision loadVarPrecision = glLastFragDataVar->getType().getPrecision();
    const unsigned int loadVarVecSize = glLastFragDataVar->getType().getNominalSize();
    const int loadVarArraySize        = glLastFragDataVar->getType().getOutermostArraySize();

    ImmutableString loadVarName("ANGLELastFragData");
    TType *loadVarType = new TType(loadVarBasicType, loadVarPrecision, EvqGlobal,
                                   static_cast<unsigned char>(loadVarVecSize));
    loadVarType->makeArray(loadVarArraySize);

    TVariable *loadVar =
        new TVariable(symbolTable, loadVarName, loadVarType, SymbolType::AngleInternal);
    replaceSubpassInputUtils.declareVariablesForFetch(kInputAttachmentZero, loadVar);

    replaceSubpassInputUtils.submitNewDeclaration();

    // 3) Add the routine for reading InputAttachment data
    if (!replaceSubpassInputUtils.loadInputAttachmentData())
    {
        return false;
    }

    // 4) Replace gl_LastFragData with ANGLELastFragData
    ReplaceVariableTraverser replaceTraverser(glLastFragDataVar, new TIntermSymbol(loadVar));
    root->traverse(&replaceTraverser);
    if (!replaceTraverser.updateTree(compiler, root))
    {
        return false;
    }

    return true;
}

ANGLE_NO_DISCARD bool ReplaceInOutVariables(TCompiler *compiler,
                                            TIntermBlock *root,
                                            TSymbolTable *symbolTable,
                                            std::vector<ShaderVariable> *uniforms)
{
    // Common variables
    InputAttachmentIdxSet constIndices;
    std::map<unsigned int, TIntermSymbol *> declaredInOutVarMap;
    unsigned int maxInputAttachmentIndex = 0;
    bool usedNonConstIndex               = false;

    // Get informations for gl_LastFragData
    InputAttachmentReferenceTraverser informationTraverser(
        &declaredInOutVarMap, &maxInputAttachmentIndex, &constIndices, &usedNonConstIndex);
    root->traverse(&informationTraverser);
    if (declaredInOutVarMap.size() == 0)
    {
        // No references of the variable decorated with a inout qualifier
        return true;
    }

    // Declare subpassInput uniform variables
    ReplaceInOutUtils replaceSubpassInputUtils(compiler, symbolTable, root, uniforms,
                                               usedNonConstIndex, constIndices,
                                               declaredInOutVarMap);
    if (!replaceSubpassInputUtils.declareSubpassInputVariables())
    {
        return false;
    }

    std::map<unsigned int, const TVariable *> toBeReplaced;
    std::map<unsigned int, const TVariable *> newOutVarArray;
    for (auto originInOutVarIter : declaredInOutVarMap)
    {
        const unsigned int inputAttachmentIndex = originInOutVarIter.first;
        const TIntermSymbol *originInOutVar     = originInOutVarIter.second;

        const TBasicType loadVarBasicType = originInOutVar->getType().getBasicType();
        const TPrecision loadVarPrecision = originInOutVar->getType().getPrecision();
        const unsigned int loadVarVecSize = originInOutVar->getType().getNominalSize();
        const unsigned int loadVarArraySize =
            (originInOutVar->isArray() ? originInOutVar->getOutermostArraySize() : 0);

        TType *newOutVarType = new TType(loadVarBasicType, loadVarPrecision, EvqGlobal,
                                         static_cast<unsigned char>(loadVarVecSize));

        // We just want to use the original variable decorated with a inout qualifier, except
        // the qualifier itself. The qualifier will be changed from inout to out.
        newOutVarType->setQualifier(TQualifier::EvqFragmentOut);

        if (loadVarArraySize > 0)
        {
            newOutVarType->makeArray(loadVarArraySize);
        }

        TVariable *newOutVar = new TVariable(symbolTable, originInOutVar->getName(), newOutVarType,
                                             SymbolType::UserDefined);
        newOutVarArray[inputAttachmentIndex] = newOutVar;
        replaceSubpassInputUtils.declareVariablesForFetch(inputAttachmentIndex,
                                                          newOutVarArray[inputAttachmentIndex]);

        toBeReplaced[inputAttachmentIndex] = &originInOutVar->variable();
    }

    replaceSubpassInputUtils.submitNewDeclaration();

    // 3) Add the routine for reading InputAttachment data
    if (!replaceSubpassInputUtils.loadInputAttachmentData())
    {
        return false;
    }

    std::map<const TVariable *, const TIntermTyped *> replacementMap;
    for (auto newOutVar = newOutVarArray.begin(); newOutVar != newOutVarArray.end(); newOutVar++)
    {
        const unsigned int index            = newOutVar->first;
        replacementMap[toBeReplaced[index]] = new TIntermSymbol(newOutVar->second);
    }

    // 4) Replace previous 'inout' variable with newly created 'inout' variable
    ReplaceVariableTraverser replaceTraverser(replacementMap);
    root->traverse(&replaceTraverser);
    if (!replaceTraverser.updateTree(compiler, root))
    {
        return false;
    }

    return true;
}

}  // namespace sh
