/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncop-exps.h - Models x86 instructions using a vector containing
 * operand trees flattened using a pre-order walk.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOP_EXPS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOP_EXPS_H_

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#include "gen/native_client/src/trusted/validator_x86/ncop_expr_node_flag.h"
#include "gen/native_client/src/trusted/validator_x86/ncop_expr_node_kind.h"

/* Defines the state used to match an instruction, while walking
 * instructions using the NcInstIter.
 */
struct NcInstState;

/* Returns the number of kids an ExprNodeKind has. */
int ExprNodeKindRank(ExprNodeKind kind);

/* Defines a bit set of ExprNodeFlags. */
typedef uint32_t ExprNodeFlags;

/* Converts an ExprNodeFlagEnum to the corresponding bit in a ExprNodesFlags
 * bit set.
 */
#define ExprFlag(x) (((ExprNodeFlags) 1) << (x))


/* Print out the set of defined expr flags. */
void PrintExprNodeFlags(FILE* file, ExprNodeFlags flags);

/* Defines a node in the vector of nodes, corresponding to the flattened
 * (preorder) tree.
 */
typedef struct ExprNode {
  /* The type of node. */
  ExprNodeKind kind;
  /* A value associated with the kind. */
  int32_t value;
  /* The set of flags associated with the node. */
  ExprNodeFlags flags;
} ExprNode;

/* Maximum number of nodes allowed in the flattened (preorder) tree. */
#define MAX_EXPR_NODES 30

/* Defines the vector of nodes, corresponding to the flattened (preorder)
 * tree that defines the instruction expression.
 */
typedef struct ExprNodeVector {
  uint32_t number_expr_nodes;
  ExprNode node[MAX_EXPR_NODES];
} ExprNodeVector;

/* Returns the number of elements in the given vector, that the subtree
 * rooted at the given node covers.
 */
int ExprNodeWidth(ExprNodeVector* vector, int node);

/* Given the given index of the node in the vector, return the index of the
 * given (zero based) kid of the node.
 */
int GetExprNodeKidIndex(ExprNodeVector* vector, int node, int kid);

/* Given an index in the vector, return the index to its parent.
 * Note: index must be > 0.
 */
int GetExprNodeParentIndex(ExprNodeVector* vector, int node);

/* Given the index of a constant, returns the corresponding constant. */
uint64_t GetExprConstant(ExprNodeVector* vector, int index);

/* Given a 64-bit constant, return the corresponding two 32-bit constants to
 * Use. Note: The lower 32 bits are put in val1, and the upper 32 bits are
 * put in val2.
 */
void SplitExprConstant(uint64_t val, uint32_t* val1, uint32_t* val2);

/* Returns true if the index points to a constant that is negative. */
Bool IsExprNegativeConstant(ExprNodeVector* vector, int index);

/* Returns the index of the i-th occurrence of the given kind of node,
 * or -1 if no such node exists.
 */
int GetNthNodeKind(ExprNodeVector* vector, ExprNodeKind kind, int n);

/* Returns the register in the given node. */
OperandKind GetNodeRegister(ExprNode* node);

/* Returns the register in the given indexed node. */
OperandKind GetNodeVectorRegister(ExprNodeVector* vector, int node);

/* Print out the contents of the given vector of nodes to the given file. */
void PrintExprNodeVector(FILE* file, ExprNodeVector* vector);

/* Print out the disassembled instruction in the given instruction state. */
void PrintNcInstStateInstruction(FILE* file, struct NcInstState* state);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCOP_EXPS_H_ */
