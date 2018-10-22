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

#include "ParseHelper.h"

//
// Use this class to carry along data from node to node in
// the traversal
//
class TConstTraverser : public TIntermTraverser {
public:
	TConstTraverser(ConstantUnion* cUnion, bool singleConstParam, TOperator constructType, TInfoSink& sink, TType& t)
		: error(false),
		  index(0),
		  unionArray(cUnion),
		  type(t),
		  constructorType(constructType),
		  singleConstantParam(singleConstParam),
		  infoSink(sink),
		  size(0),
		  isMatrix(false),
		  matrixSize(0) {
	}

	bool error;

protected:
	void visitSymbol(TIntermSymbol*);
	void visitConstantUnion(TIntermConstantUnion*);
	bool visitBinary(Visit visit, TIntermBinary*);
	bool visitUnary(Visit visit, TIntermUnary*);
	bool visitSelection(Visit visit, TIntermSelection*);
	bool visitAggregate(Visit visit, TIntermAggregate*);
	bool visitLoop(Visit visit, TIntermLoop*);
	bool visitBranch(Visit visit, TIntermBranch*);

	size_t index;
	ConstantUnion *unionArray;
	TType type;
	TOperator constructorType;
	bool singleConstantParam;
	TInfoSink& infoSink;
	size_t size; // size of the constructor ( 4 for vec4)
	bool isMatrix;
	int matrixSize; // dimension of the matrix (nominal size and not the instance size)
};

//
// The rest of the file are the traversal functions.  The last one
// is the one that starts the traversal.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  If you process children yourself,
// return false.
//

void TConstTraverser::visitSymbol(TIntermSymbol* node)
{
	infoSink.info.message(EPrefixInternalError, "Symbol Node found in constant constructor", node->getLine());
	return;
}

bool TConstTraverser::visitBinary(Visit visit, TIntermBinary* node)
{
	TQualifier qualifier = node->getType().getQualifier();

	if (qualifier != EvqConstExpr) {
		TString buf;
		buf.append("'constructor' : assigning non-constant to ");
		buf.append(type.getCompleteString());
		infoSink.info.message(EPrefixError, buf.c_str(), node->getLine());
		error = true;
		return false;
	}

	infoSink.info.message(EPrefixInternalError, "Binary Node found in constant constructor", node->getLine());

	return false;
}

bool TConstTraverser::visitUnary(Visit visit, TIntermUnary* node)
{
	TString buf;
	buf.append("'constructor' : assigning non-constant to ");
	buf.append(type.getCompleteString());
	infoSink.info.message(EPrefixError, buf.c_str(), node->getLine());
	error = true;
	return false;
}

bool TConstTraverser::visitAggregate(Visit visit, TIntermAggregate* node)
{
	if (!node->isConstructor() && node->getOp() != EOpComma) {
		TString buf;
		buf.append("'constructor' : assigning non-constant to ");
		buf.append(type.getCompleteString());
		infoSink.info.message(EPrefixError, buf.c_str(), node->getLine());
		error = true;
		return false;
	}

	if (node->getSequence().size() == 0) {
		error = true;
		return false;
	}

	bool flag = node->getSequence().size() == 1 && node->getSequence()[0]->getAsTyped()->getAsConstantUnion();
	if (flag)
	{
		singleConstantParam = true;
		constructorType = node->getOp();
		size = node->getType().getObjectSize();

		if (node->getType().isMatrix()) {
			isMatrix = true;
			matrixSize = node->getType().getNominalSize();
		}
	}

	for (TIntermSequence::iterator p = node->getSequence().begin();
	                               p != node->getSequence().end(); p++) {

		if (node->getOp() == EOpComma)
			index = 0;

		(*p)->traverse(this);
	}
	if (flag)
	{
		singleConstantParam = false;
		constructorType = EOpNull;
		size = 0;
		isMatrix = false;
		matrixSize = 0;
	}
	return false;
}

bool TConstTraverser::visitSelection(Visit visit, TIntermSelection* node)
{
	infoSink.info.message(EPrefixInternalError, "Selection Node found in constant constructor", node->getLine());
	error = true;
	return false;
}

void TConstTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
	if (!node->getUnionArrayPointer())
	{
		// The constant was not initialized, this should already have been logged
		assert(infoSink.info.size() != 0);
		return;
	}

	ConstantUnion* leftUnionArray = unionArray;
	size_t instanceSize = type.getObjectSize();
	TBasicType basicType = type.getBasicType();

	if (index >= instanceSize)
		return;

	if (!singleConstantParam) {
		size_t size = node->getType().getObjectSize();

		ConstantUnion *rightUnionArray = node->getUnionArrayPointer();
		for(size_t i = 0; i < size; i++) {
			if (index >= instanceSize)
				return;
			leftUnionArray[index].cast(basicType, rightUnionArray[i]);

			(index)++;
		}
	} else {
		size_t totalSize = index + size;
		ConstantUnion *rightUnionArray = node->getUnionArrayPointer();
		if (!isMatrix) {
			int count = 0;
			for(size_t i = index; i < totalSize; i++) {
				if (i >= instanceSize)
					return;

				leftUnionArray[i].cast(basicType, rightUnionArray[count]);

				(index)++;

				if (node->getType().getObjectSize() > 1)
					count++;
			}
		} else {  // for matrix constructors
			int count = 0;
			int element = index;
			for(size_t i = index; i < totalSize; i++) {
				if (i >= instanceSize)
					return;
				if (element - i == 0 || (i - element) % (matrixSize + 1) == 0 )
					leftUnionArray[i].cast(basicType, rightUnionArray[0]);
				else
					leftUnionArray[i].setFConst(0.0f);

				(index)++;

				if (node->getType().getObjectSize() > 1)
					count++;
			}
		}
	}
}

bool TConstTraverser::visitLoop(Visit visit, TIntermLoop* node)
{
	infoSink.info.message(EPrefixInternalError, "Loop Node found in constant constructor", node->getLine());
	error = true;
	return false;
}

bool TConstTraverser::visitBranch(Visit visit, TIntermBranch* node)
{
	infoSink.info.message(EPrefixInternalError, "Branch Node found in constant constructor", node->getLine());
	error = true;
	return false;
}

//
// This function is the one to call externally to start the traversal.
// Individual functions can be initialized to 0 to skip processing of that
// type of node.  It's children will still be processed.
//
bool TIntermediate::parseConstTree(const TSourceLoc& line, TIntermNode* root, ConstantUnion* unionArray, TOperator constructorType, TType t, bool singleConstantParam)
{
	if (root == 0)
		return false;

	TConstTraverser it(unionArray, singleConstantParam, constructorType, infoSink, t);

	root->traverse(&it);
	if (it.error)
		return true;
	else
		return false;
}
