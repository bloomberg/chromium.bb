//===-- Local.h - Functions to perform local transformations ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This family of functions perform various local transformations to the
// program.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_LOCAL_H
#define LLVM_TRANSFORMS_UTILS_LOCAL_H

namespace llvm {

class User;
class BasicBlock;
class Function;
class BranchInst;
class Instruction;
class DbgDeclareInst;
class StoreInst;
class LoadInst;
class Value;
class Pass;
class PHINode;
class AllocaInst;
class ConstantExpr;
class TargetData;
class DIBuilder;

template<typename T> class SmallVectorImpl;
  
//===----------------------------------------------------------------------===//
//  Local constant propagation.
//

/// ConstantFoldTerminator - If a terminator instruction is predicated on a
/// constant value, convert it into an unconditional branch to the constant
/// destination.  This is a nontrivial operation because the successors of this
/// basic block must have their PHI nodes updated.
/// Also calls RecursivelyDeleteTriviallyDeadInstructions() on any branch/switch
/// conditions and indirectbr addresses this might make dead if
/// DeleteDeadConditions is true.
bool ConstantFoldTerminator(BasicBlock *BB, bool DeleteDeadConditions = false);

//===----------------------------------------------------------------------===//
//  Local dead code elimination.
//

/// isInstructionTriviallyDead - Return true if the result produced by the
/// instruction is not used, and the instruction has no side effects.
///
bool isInstructionTriviallyDead(Instruction *I);

/// RecursivelyDeleteTriviallyDeadInstructions - If the specified value is a
/// trivially dead instruction, delete it.  If that makes any of its operands
/// trivially dead, delete them too, recursively.  Return true if any
/// instructions were deleted.
bool RecursivelyDeleteTriviallyDeadInstructions(Value *V);

/// RecursivelyDeleteDeadPHINode - If the specified value is an effectively
/// dead PHI node, due to being a def-use chain of single-use nodes that
/// either forms a cycle or is terminated by a trivially dead instruction,
/// delete it.  If that makes any of its operands trivially dead, delete them
/// too, recursively.  Return true if a change was made.
bool RecursivelyDeleteDeadPHINode(PHINode *PN);

  
/// SimplifyInstructionsInBlock - Scan the specified basic block and try to
/// simplify any instructions in it and recursively delete dead instructions.
///
/// This returns true if it changed the code, note that it can delete
/// instructions in other blocks as well in this block.
bool SimplifyInstructionsInBlock(BasicBlock *BB, const TargetData *TD = 0);
    
//===----------------------------------------------------------------------===//
//  Control Flow Graph Restructuring.
//

/// RemovePredecessorAndSimplify - Like BasicBlock::removePredecessor, this
/// method is called when we're about to delete Pred as a predecessor of BB.  If
/// BB contains any PHI nodes, this drops the entries in the PHI nodes for Pred.
///
/// Unlike the removePredecessor method, this attempts to simplify uses of PHI
/// nodes that collapse into identity values.  For example, if we have:
///   x = phi(1, 0, 0, 0)
///   y = and x, z
///
/// .. and delete the predecessor corresponding to the '1', this will attempt to
/// recursively fold the 'and' to 0.
void RemovePredecessorAndSimplify(BasicBlock *BB, BasicBlock *Pred,
                                  TargetData *TD = 0);
    
  
/// MergeBasicBlockIntoOnlyPred - BB is a block with one predecessor and its
/// predecessor is known to have one successor (BB!).  Eliminate the edge
/// between them, moving the instructions in the predecessor into BB.  This
/// deletes the predecessor block.
///
void MergeBasicBlockIntoOnlyPred(BasicBlock *BB, Pass *P = 0);
    

/// TryToSimplifyUncondBranchFromEmptyBlock - BB is known to contain an
/// unconditional branch, and contains no instructions other than PHI nodes,
/// potential debug intrinsics and the branch.  If possible, eliminate BB by
/// rewriting all the predecessors to branch to the successor block and return
/// true.  If we can't transform, return false.
bool TryToSimplifyUncondBranchFromEmptyBlock(BasicBlock *BB);

/// EliminateDuplicatePHINodes - Check for and eliminate duplicate PHI
/// nodes in this block. This doesn't try to be clever about PHI nodes
/// which differ only in the order of the incoming values, but instcombine
/// orders them so it usually won't matter.
///
bool EliminateDuplicatePHINodes(BasicBlock *BB);

/// SimplifyCFG - This function is used to do simplification of a CFG.  For
/// example, it adjusts branches to branches to eliminate the extra hop, it
/// eliminates unreachable basic blocks, and does other "peephole" optimization
/// of the CFG.  It returns true if a modification was made, possibly deleting
/// the basic block that was pointed to.
///
bool SimplifyCFG(BasicBlock *BB, const TargetData *TD = 0);

/// FoldBranchToCommonDest - If this basic block is ONLY a setcc and a branch,
/// and if a predecessor branches to us and one of our successors, fold the
/// setcc into the predecessor and use logical operations to pick the right
/// destination.
bool FoldBranchToCommonDest(BranchInst *BI);

/// DemoteRegToStack - This function takes a virtual register computed by an
/// Instruction and replaces it with a slot in the stack frame, allocated via
/// alloca.  This allows the CFG to be changed around without fear of
/// invalidating the SSA information for the value.  It returns the pointer to
/// the alloca inserted to create a stack slot for X.
///
AllocaInst *DemoteRegToStack(Instruction &X,
                             bool VolatileLoads = false,
                             Instruction *AllocaPoint = 0);

/// DemotePHIToStack - This function takes a virtual register computed by a phi
/// node and replaces it with a slot in the stack frame, allocated via alloca.
/// The phi node is deleted and it returns the pointer to the alloca inserted. 
AllocaInst *DemotePHIToStack(PHINode *P, Instruction *AllocaPoint = 0);

/// getOrEnforceKnownAlignment - If the specified pointer has an alignment that
/// we can determine, return it, otherwise return 0.  If PrefAlign is specified,
/// and it is more than the alignment of the ultimate object, see if we can
/// increase the alignment of the ultimate object, making this check succeed.
unsigned getOrEnforceKnownAlignment(Value *V, unsigned PrefAlign,
                                    const TargetData *TD = 0);

/// getKnownAlignment - Try to infer an alignment for the specified pointer.
static inline unsigned getKnownAlignment(Value *V, const TargetData *TD = 0) {
  return getOrEnforceKnownAlignment(V, 0, TD);
}

///===---------------------------------------------------------------------===//
///  Dbg Intrinsic utilities
///

/// Inserts a llvm.dbg.value instrinsic before the stores to an alloca'd value
/// that has an associated llvm.dbg.decl intrinsic.
bool ConvertDebugDeclareToDebugValue(DbgDeclareInst *DDI,
                                     StoreInst *SI, DIBuilder &Builder);

/// Inserts a llvm.dbg.value instrinsic before the stores to an alloca'd value
/// that has an associated llvm.dbg.decl intrinsic.
bool ConvertDebugDeclareToDebugValue(DbgDeclareInst *DDI,
                                     LoadInst *LI, DIBuilder &Builder);

/// LowerDbgDeclare - Lowers llvm.dbg.declare intrinsics into appropriate set
/// of llvm.dbg.value intrinsics.
bool LowerDbgDeclare(Function &F);

/// FindAllocaDbgDeclare - Finds the llvm.dbg.declare intrinsic corresponding to
/// an alloca, if any.
DbgDeclareInst *FindAllocaDbgDeclare(Value *V);

} // End llvm namespace

#endif
