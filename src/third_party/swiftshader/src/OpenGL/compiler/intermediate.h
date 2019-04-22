// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Definition of the in-memory high-level intermediate representation
// of shaders.  This is a tree that parser creates.
//
// Nodes in the tree are defined as a hierarchy of classes derived from
// TIntermNode. Each is a node in a tree.  There is no preset branching factor;
// each node can have it's own type of list of children.
//

#ifndef __INTERMEDIATE_H
#define __INTERMEDIATE_H

#include "Common.h"
#include "Types.h"
#include "ConstantUnion.h"

//
// Operators used by the high-level (parse tree) representation.
//
enum TOperator {
	EOpNull,            // if in a node, should only mean a node is still being built
	EOpSequence,        // denotes a list of statements, or parameters, etc.
	EOpFunctionCall,
	EOpFunction,        // For function definition
	EOpParameters,      // an aggregate listing the parameters to a function

	EOpDeclaration,
	EOpInvariantDeclaration, // Specialized declarations for attributing invariance
	EOpPrototype,

	//
	// Unary operators
	//

	EOpNegative,
	EOpLogicalNot,
	EOpVectorLogicalNot,
	EOpBitwiseNot,

	EOpPostIncrement,
	EOpPostDecrement,
	EOpPreIncrement,
	EOpPreDecrement,

	//
	// binary operations
	//

	EOpAdd,
	EOpSub,
	EOpMul,
	EOpDiv,
	EOpEqual,
	EOpNotEqual,
	EOpVectorEqual,
	EOpVectorNotEqual,
	EOpLessThan,
	EOpGreaterThan,
	EOpLessThanEqual,
	EOpGreaterThanEqual,
	EOpComma,

	EOpOuterProduct,
	EOpTranspose,
	EOpDeterminant,
	EOpInverse,

	EOpVectorTimesScalar,
	EOpVectorTimesMatrix,
	EOpMatrixTimesVector,
	EOpMatrixTimesScalar,

	EOpLogicalOr,
	EOpLogicalXor,
	EOpLogicalAnd,

	EOpIMod,
	EOpBitShiftLeft,
	EOpBitShiftRight,
	EOpBitwiseAnd,
	EOpBitwiseXor,
	EOpBitwiseOr,

	EOpIndexDirect,
	EOpIndexIndirect,
	EOpIndexDirectStruct,
	EOpIndexDirectInterfaceBlock,

	EOpVectorSwizzle,

	//
	// Built-in functions potentially mapped to operators
	//

	EOpRadians,
	EOpDegrees,
	EOpSin,
	EOpCos,
	EOpTan,
	EOpAsin,
	EOpAcos,
	EOpAtan,
	EOpSinh,
	EOpCosh,
	EOpTanh,
	EOpAsinh,
	EOpAcosh,
	EOpAtanh,

	EOpPow,
	EOpExp,
	EOpLog,
	EOpExp2,
	EOpLog2,
	EOpSqrt,
	EOpInverseSqrt,

	EOpAbs,
	EOpSign,
	EOpFloor,
	EOpTrunc,
	EOpRound,
	EOpRoundEven,
	EOpCeil,
	EOpFract,
	EOpMod,
	EOpModf,
	EOpMin,
	EOpMax,
	EOpClamp,
	EOpMix,
	EOpStep,
	EOpSmoothStep,
	EOpIsNan,
	EOpIsInf,
	EOpFloatBitsToInt,
	EOpFloatBitsToUint,
	EOpIntBitsToFloat,
	EOpUintBitsToFloat,
	EOpPackSnorm2x16,
	EOpPackUnorm2x16,
	EOpPackHalf2x16,
	EOpUnpackSnorm2x16,
	EOpUnpackUnorm2x16,
	EOpUnpackHalf2x16,

	EOpLength,
	EOpDistance,
	EOpDot,
	EOpCross,
	EOpNormalize,
	EOpFaceForward,
	EOpReflect,
	EOpRefract,

	EOpDFdx,            // Fragment only, OES_standard_derivatives extension
	EOpDFdy,            // Fragment only, OES_standard_derivatives extension
	EOpFwidth,          // Fragment only, OES_standard_derivatives extension

	EOpMatrixTimesMatrix,

	EOpAny,
	EOpAll,

	//
	// Branch
	//

	EOpKill,            // Fragment only
	EOpReturn,
	EOpBreak,
	EOpContinue,

	//
	// Constructors
	//

	EOpConstructInt,
	EOpConstructUInt,
	EOpConstructBool,
	EOpConstructFloat,
	EOpConstructVec2,
	EOpConstructVec3,
	EOpConstructVec4,
	EOpConstructBVec2,
	EOpConstructBVec3,
	EOpConstructBVec4,
	EOpConstructIVec2,
	EOpConstructIVec3,
	EOpConstructIVec4,
	EOpConstructUVec2,
	EOpConstructUVec3,
	EOpConstructUVec4,
	EOpConstructMat2,
	EOpConstructMat2x3,
	EOpConstructMat2x4,
	EOpConstructMat3x2,
	EOpConstructMat3,
	EOpConstructMat3x4,
	EOpConstructMat4x2,
	EOpConstructMat4x3,
	EOpConstructMat4,
	EOpConstructStruct,

	//
	// moves
	//

	EOpAssign,
	EOpInitialize,
	EOpAddAssign,
	EOpSubAssign,
	EOpMulAssign,
	EOpVectorTimesMatrixAssign,
	EOpVectorTimesScalarAssign,
	EOpMatrixTimesScalarAssign,
	EOpMatrixTimesMatrixAssign,
	EOpDivAssign,
	EOpIModAssign,
	EOpBitShiftLeftAssign,
	EOpBitShiftRightAssign,
	EOpBitwiseAndAssign,
	EOpBitwiseXorAssign,
	EOpBitwiseOrAssign
};

extern TOperator TypeToConstructorOperator(const TType &type);
extern const char* getOperatorString(TOperator op);

class TIntermTraverser;
class TIntermAggregate;
class TIntermBinary;
class TIntermUnary;
class TIntermConstantUnion;
class TIntermSelection;
class TIntermTyped;
class TIntermSymbol;
class TIntermLoop;
class TIntermBranch;
class TInfoSink;
class TIntermSwitch;
class TIntermCase;

//
// Base class for the tree nodes
//
class TIntermNode {
public:
	POOL_ALLOCATOR_NEW_DELETE()

	TIntermNode()
	{
		// TODO: Move this to TSourceLoc constructor
		// after getting rid of TPublicType.
		line.first_file = line.last_file = 0;
		line.first_line = line.last_line = 0;
	}

	const TSourceLoc& getLine() const { return line; }
	void setLine(const TSourceLoc& l) { line = l; }

	virtual void traverse(TIntermTraverser*) = 0;
	virtual TIntermTyped* getAsTyped() { return 0; }
	virtual TIntermConstantUnion* getAsConstantUnion() { return 0; }
	virtual TIntermAggregate* getAsAggregate() { return 0; }
	virtual TIntermBinary* getAsBinaryNode() { return 0; }
	virtual TIntermUnary* getAsUnaryNode() { return 0; }
	virtual TIntermSelection* getAsSelectionNode() { return 0; }
	virtual TIntermSymbol* getAsSymbolNode() { return 0; }
	virtual TIntermLoop* getAsLoopNode() { return 0; }
	virtual TIntermBranch* getAsBranchNode() { return 0; }
	virtual TIntermSwitch *getAsSwitchNode() { return 0; }
	virtual TIntermCase *getAsCaseNode() { return 0; }
	virtual ~TIntermNode() { }

protected:
	TSourceLoc line;
};

//
// This is just to help yacc.
//
struct TIntermNodePair {
	TIntermNode* node1;
	TIntermNode* node2;
};

//
// Intermediate class for nodes that have a type.
//
class TIntermTyped : public TIntermNode {
public:
	TIntermTyped(const TType& t) : type(t)  { }
	virtual TIntermTyped* getAsTyped() { return this; }

	virtual void setType(const TType& t) { type = t; }
	const TType& getType() const { return type; }
	TType* getTypePointer() { return &type; }

	TBasicType getBasicType() const { return type.getBasicType(); }
	TQualifier getQualifier() const { return type.getQualifier(); }
	TPrecision getPrecision() const { return type.getPrecision(); }
	int getNominalSize() const { return type.getNominalSize(); }
	int getSecondarySize() const { return type.getSecondarySize(); }

	bool isInterfaceBlock() const { return type.isInterfaceBlock(); }
	bool isMatrix() const { return type.isMatrix(); }
	bool isArray()  const { return type.isArray(); }
	bool isVector() const { return type.isVector(); }
	bool isScalar() const { return type.isScalar(); }
	bool isScalarInt() const { return type.isScalarInt(); }
	bool isRegister() const { return type.isRegister(); }   // Fits in a 4-element register
	bool isStruct() const { return type.isStruct(); }
	const char* getBasicString() const { return type.getBasicString(); }
	const char* getQualifierString() const { return type.getQualifierString(); }
	TString getCompleteString() const { return type.getCompleteString(); }

	int totalRegisterCount() const { return type.totalRegisterCount(); }
	int blockRegisterCount(bool samplersOnly) const { return samplersOnly ? type.totalSamplerRegisterCount() : type.blockRegisterCount(); }
	int elementRegisterCount() const { return type.elementRegisterCount(); }
	int registerSize() const { return type.registerSize(); }
	int getArraySize() const { return type.getArraySize(); }

	static TIntermTyped *CreateIndexNode(int index);
protected:
	TType type;
};

//
// Handle for, do-while, and while loops.
//
enum TLoopType {
	ELoopFor,
	ELoopWhile,
	ELoopDoWhile
};

class TIntermLoop : public TIntermNode {
public:
	TIntermLoop(TLoopType aType,
	            TIntermNode *aInit, TIntermTyped* aCond, TIntermTyped* aExpr,
	            TIntermNode* aBody) :
			type(aType),
			init(aInit),
			cond(aCond),
			expr(aExpr),
			body(aBody),
			unrollFlag(false) { }

	virtual TIntermLoop* getAsLoopNode() { return this; }
	virtual void traverse(TIntermTraverser*);

	TLoopType getType() const { return type; }
	TIntermNode* getInit() { return init; }
	TIntermTyped* getCondition() { return cond; }
	TIntermTyped* getExpression() { return expr; }
	TIntermNode* getBody() { return body; }

	void setUnrollFlag(bool flag) { unrollFlag = flag; }
	bool getUnrollFlag() { return unrollFlag; }

protected:
	TLoopType type;
	TIntermNode* init;  // for-loop initialization
	TIntermTyped* cond; // loop exit condition
	TIntermTyped* expr; // for-loop expression
	TIntermNode* body;  // loop body

	bool unrollFlag; // Whether the loop should be unrolled or not.
};

//
// Handle break, continue, return, and kill.
//
class TIntermBranch : public TIntermNode {
public:
	TIntermBranch(TOperator op, TIntermTyped* e) :
			flowOp(op),
			expression(e) { }

	virtual TIntermBranch* getAsBranchNode() { return this; }
	virtual void traverse(TIntermTraverser*);

	TOperator getFlowOp() { return flowOp; }
	TIntermTyped* getExpression() { return expression; }

protected:
	TOperator flowOp;
	TIntermTyped* expression;  // non-zero except for "return exp;" statements
};

//
// Nodes that correspond to symbols or constants in the source code.
//
class TIntermSymbol : public TIntermTyped {
public:
	// if symbol is initialized as symbol(sym), the memory comes from the poolallocator of sym. If sym comes from
	// per process globalpoolallocator, then it causes increased memory usage per compile
	// it is essential to use "symbol = sym" to assign to symbol
	TIntermSymbol(int i, const TString& sym, const TType& t) :
			TIntermTyped(t), id(i)  { symbol = sym; }

	int getId() const { return id; }
	const TString& getSymbol() const { return symbol; }

	void setId(int newId) { id = newId; }

	virtual void traverse(TIntermTraverser*);
	virtual TIntermSymbol* getAsSymbolNode() { return this; }

protected:
	int id;
	TString symbol;
};

class TIntermConstantUnion : public TIntermTyped {
public:
	TIntermConstantUnion(ConstantUnion *unionPointer, const TType& t) : TIntermTyped(t), unionArrayPointer(unionPointer)
	{
		getTypePointer()->setQualifier(EvqConstExpr);
	}

	ConstantUnion* getUnionArrayPointer() const { return unionArrayPointer; }

	int getIConst(int index) const { return unionArrayPointer ? unionArrayPointer[index].getIConst() : 0; }
	int getUConst(int index) const { return unionArrayPointer ? unionArrayPointer[index].getUConst() : 0; }
	float getFConst(int index) const { return unionArrayPointer ? unionArrayPointer[index].getFConst() : 0.0f; }
	bool getBConst(int index) const { return unionArrayPointer ? unionArrayPointer[index].getBConst() : false; }

	// Previous union pointer freed on pool deallocation.
	void replaceConstantUnion(ConstantUnion *safeConstantUnion) { unionArrayPointer = safeConstantUnion; }

	virtual TIntermConstantUnion* getAsConstantUnion()  { return this; }
	virtual void traverse(TIntermTraverser*);

	TIntermTyped* fold(TOperator, TIntermTyped*, TInfoSink&);

protected:
	ConstantUnion *unionArrayPointer;
};

//
// Intermediate class for node types that hold operators.
//
class TIntermOperator : public TIntermTyped {
public:
	TOperator getOp() const { return op; }
	void setOp(TOperator o) { op = o; }

	bool modifiesState() const;
	bool isConstructor() const;

protected:
	TIntermOperator(TOperator o) : TIntermTyped(TType(EbtFloat, EbpUndefined)), op(o) {}
	TIntermOperator(TOperator o, TType& t) : TIntermTyped(t), op(o) {}
	TOperator op;
};

//
// Nodes for all the basic binary math operators.
//
class TIntermBinary : public TIntermOperator {
public:
	TIntermBinary(TOperator o) : TIntermOperator(o) {}

	TIntermBinary* getAsBinaryNode() override { return this; }
	void traverse(TIntermTraverser*) override;

	void setType(const TType &t) override
	{
		type = t;

		if(left->getQualifier() == EvqConstExpr && right->getQualifier() == EvqConstExpr)
		{
			type.setQualifier(EvqConstExpr);
		}
	}

	void setLeft(TIntermTyped* n) { left = n; }
	void setRight(TIntermTyped* n) { right = n; }
	TIntermTyped* getLeft() const { return left; }
	TIntermTyped* getRight() const { return right; }
	bool promote(TInfoSink&);

protected:
	TIntermTyped* left;
	TIntermTyped* right;
};

//
// Nodes for unary math operators.
//
class TIntermUnary : public TIntermOperator {
public:
	TIntermUnary(TOperator o, TType& t) : TIntermOperator(o, t), operand(0) {}
	TIntermUnary(TOperator o) : TIntermOperator(o), operand(0) {}

	void setType(const TType &t) override
	{
		type = t;

		if(operand->getQualifier() == EvqConstExpr)
		{
			type.setQualifier(EvqConstExpr);
		}
	}

	void traverse(TIntermTraverser*) override;
	TIntermUnary* getAsUnaryNode() override { return this; }

	void setOperand(TIntermTyped* o) { operand = o; }
	TIntermTyped* getOperand() { return operand; }
	bool promote(TInfoSink&, const TType *funcReturnType);

protected:
	TIntermTyped* operand;
};

typedef TVector<TIntermNode*> TIntermSequence;
typedef TVector<int> TQualifierList;

//
// Nodes that operate on an arbitrary sized set of children.
//
class TIntermAggregate : public TIntermOperator {
public:
	TIntermAggregate() : TIntermOperator(EOpNull), userDefined(false) { endLine = { 0, 0, 0, 0 }; }
	TIntermAggregate(TOperator o) : TIntermOperator(o), userDefined(false) { endLine = { 0, 0, 0, 0 }; }
	~TIntermAggregate() { }

	TIntermAggregate* getAsAggregate() override { return this; }
	void traverse(TIntermTraverser*) override;

	TIntermSequence& getSequence() { return sequence; }

	void setType(const TType &t) override
	{
		type = t;

		if(op != EOpFunctionCall)
		{
			for(TIntermNode *node : sequence)
			{
				if(!node->getAsTyped() || node->getAsTyped()->getQualifier() != EvqConstExpr)
				{
					return;
				}
			}

			type.setQualifier(EvqConstExpr);
		}
	}

	void setName(const TString& n) { name = n; }
	const TString& getName() const { return name; }

	void setUserDefined() { userDefined = true; }
	bool isUserDefined() const { return userDefined; }

	void setOptimize(bool o) { optimize = o; }
	bool getOptimize() { return optimize; }
	void setDebug(bool d) { debug = d; }
	bool getDebug() { return debug; }

	void setEndLine(const TSourceLoc& line) { endLine = line; }
	const TSourceLoc& getEndLine() const { return endLine; }

	bool isConstantFoldable()
	{
		for(TIntermNode *node : sequence)
		{
			if(!node->getAsConstantUnion() || !node->getAsConstantUnion()->getUnionArrayPointer())
			{
				return false;
			}
		}

		return true;
	}

protected:
	TIntermAggregate(const TIntermAggregate&); // disallow copy constructor
	TIntermAggregate& operator=(const TIntermAggregate&); // disallow assignment operator
	TIntermSequence sequence;
	TString name;
	bool userDefined; // used for user defined function names

	bool optimize;
	bool debug;
	TSourceLoc endLine;
};

//
// For if tests.  Simplified since there is no switch statement.
//
class TIntermSelection : public TIntermTyped {
public:
	TIntermSelection(TIntermTyped* cond, TIntermNode* trueB, TIntermNode* falseB) :
			TIntermTyped(TType(EbtVoid, EbpUndefined)), condition(cond), trueBlock(trueB), falseBlock(falseB) {}
	TIntermSelection(TIntermTyped* cond, TIntermNode* trueB, TIntermNode* falseB, const TType& type) :
			TIntermTyped(type), condition(cond), trueBlock(trueB), falseBlock(falseB)
	{
		this->type.setQualifier(EvqTemporary);
	}

	virtual void traverse(TIntermTraverser*);

	bool usesTernaryOperator() const { return getBasicType() != EbtVoid; }
	TIntermTyped* getCondition() const { return condition; }
	TIntermNode* getTrueBlock() const { return trueBlock; }
	TIntermNode* getFalseBlock() const { return falseBlock; }
	TIntermSelection* getAsSelectionNode() { return this; }

protected:
	TIntermTyped* condition;
	TIntermNode* trueBlock;
	TIntermNode* falseBlock;
};

//
// Switch statement.
//
class TIntermSwitch : public TIntermNode
{
public:
	TIntermSwitch(TIntermTyped *init, TIntermAggregate *statementList)
		: TIntermNode(), mInit(init), mStatementList(statementList)
	{}

	void traverse(TIntermTraverser *it);

	TIntermSwitch *getAsSwitchNode() { return this; }

	TIntermTyped *getInit() { return mInit; }
	TIntermAggregate *getStatementList() { return mStatementList; }
	void setStatementList(TIntermAggregate *statementList) { mStatementList = statementList; }

protected:
	TIntermTyped *mInit;
	TIntermAggregate *mStatementList;
};

//
// Case label.
//
class TIntermCase : public TIntermNode
{
public:
	TIntermCase(TIntermTyped *condition)
		: TIntermNode(), mCondition(condition)
	{}

	void traverse(TIntermTraverser *it);

	TIntermCase *getAsCaseNode() { return this; }

	bool hasCondition() const { return mCondition != nullptr; }
	TIntermTyped *getCondition() const { return mCondition; }

protected:
	TIntermTyped *mCondition;
};

enum Visit
{
	PreVisit,
	InVisit,
	PostVisit
};

//
// For traversing the tree.  User should derive from this,
// put their traversal specific data in it, and then pass
// it to a Traverse method.
//
// When using this, just fill in the methods for nodes you want visited.
// Return false from a pre-visit to skip visiting that node's subtree.
//
class TIntermTraverser
{
public:
	POOL_ALLOCATOR_NEW_DELETE()
	TIntermTraverser(bool preVisit = true, bool inVisit = false, bool postVisit = false, bool rightToLeft = false) :
			preVisit(preVisit),
			inVisit(inVisit),
			postVisit(postVisit),
			rightToLeft(rightToLeft),
			mDepth(0) {}
	virtual ~TIntermTraverser() {}

	virtual void visitSymbol(TIntermSymbol*) {}
	virtual void visitConstantUnion(TIntermConstantUnion*) {}
	virtual bool visitBinary(Visit visit, TIntermBinary*) {return true;}
	virtual bool visitUnary(Visit visit, TIntermUnary*) {return true;}
	virtual bool visitSelection(Visit visit, TIntermSelection*) {return true;}
	virtual bool visitAggregate(Visit visit, TIntermAggregate*) {return true;}
	virtual bool visitLoop(Visit visit, TIntermLoop*) {return true;}
	virtual bool visitBranch(Visit visit, TIntermBranch*) {return true;}
	virtual bool visitSwitch(Visit, TIntermSwitch*) { return true; }
	virtual bool visitCase(Visit, TIntermCase*) { return true; }

	void incrementDepth(TIntermNode *current)
	{
		mDepth++;
		mPath.push_back(current);
	}

	void decrementDepth()
	{
		mDepth--;
		mPath.pop_back();
	}

	TIntermNode *getParentNode()
	{
		return mPath.size() == 0 ? nullptr : mPath.back();
	}

	const bool preVisit;
	const bool inVisit;
	const bool postVisit;
	const bool rightToLeft;

protected:
	int mDepth;

	// All the nodes from root to the current node's parent during traversing.
	TVector<TIntermNode *> mPath;

private:
	struct ParentBlock
	{
		ParentBlock(TIntermAggregate *nodeIn, TIntermSequence::size_type posIn)
		: node(nodeIn), pos(posIn)
		{}

		TIntermAggregate *node;
		TIntermSequence::size_type pos;
	};
	// All the code blocks from the root to the current node's parent during traversal.
	std::vector<ParentBlock> mParentBlockStack;
};

#endif // __INTERMEDIATE_H
