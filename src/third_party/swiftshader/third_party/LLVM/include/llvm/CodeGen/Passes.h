//===-- Passes.h - Target independent code generation passes ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines interfaces to access the target independent code generation
// passes provided by the LLVM backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_PASSES_H
#define LLVM_CODEGEN_PASSES_H

#include "llvm/Target/TargetMachine.h"
#include <string>

namespace llvm {

  class FunctionPass;
  class MachineFunctionPass;
  class PassInfo;
  class TargetLowering;
  class TargetRegisterClass;
  class raw_ostream;

  /// createUnreachableBlockEliminationPass - The LLVM code generator does not
  /// work well with unreachable basic blocks (what live ranges make sense for a
  /// block that cannot be reached?).  As such, a code generator should either
  /// not instruction select unreachable blocks, or run this pass as its
  /// last LLVM modifying pass to clean up blocks that are not reachable from
  /// the entry block.
  FunctionPass *createUnreachableBlockEliminationPass();

  /// MachineFunctionPrinter pass - This pass prints out the machine function to
  /// the given stream as a debugging tool.
  MachineFunctionPass *
  createMachineFunctionPrinterPass(raw_ostream &OS,
                                   const std::string &Banner ="");

  /// MachineLoopInfo pass - This pass is a loop analysis pass.
  ///
  extern char &MachineLoopInfoID;

  /// MachineLoopRanges pass - This pass is an on-demand loop coverage
  /// analysis pass.
  ///
  extern char &MachineLoopRangesID;

  /// MachineDominators pass - This pass is a machine dominators analysis pass.
  ///
  extern char &MachineDominatorsID;

  /// EdgeBundles analysis - Bundle machine CFG edges.
  ///
  extern char &EdgeBundlesID;

  /// PHIElimination pass - This pass eliminates machine instruction PHI nodes
  /// by inserting copy instructions.  This destroys SSA information, but is the
  /// desired input for some register allocators.  This pass is "required" by
  /// these register allocator like this: AU.addRequiredID(PHIEliminationID);
  ///
  extern char &PHIEliminationID;

  /// StrongPHIElimination pass - This pass eliminates machine instruction PHI
  /// nodes by inserting copy instructions.  This destroys SSA information, but
  /// is the desired input for some register allocators.  This pass is
  /// "required" by these register allocator like this:
  ///    AU.addRequiredID(PHIEliminationID);
  ///  This pass is still in development
  extern char &StrongPHIEliminationID;

  /// LiveStacks pass. An analysis keeping track of the liveness of stack slots.
  extern char &LiveStacksID;

  /// TwoAddressInstruction pass - This pass reduces two-address instructions to
  /// use two operands. This destroys SSA information but it is desired by
  /// register allocators.
  extern char &TwoAddressInstructionPassID;

  /// RegisteCoalescer pass - This pass merges live ranges to eliminate copies.
  extern char &RegisterCoalescerPassID;

  /// SpillPlacement analysis. Suggest optimal placement of spill code between
  /// basic blocks.
  ///
  extern char &SpillPlacementID;

  /// UnreachableMachineBlockElimination pass - This pass removes unreachable
  /// machine basic blocks.
  extern char &UnreachableMachineBlockElimID;

  /// DeadMachineInstructionElim pass - This pass removes dead machine
  /// instructions.
  ///
  FunctionPass *createDeadMachineInstructionElimPass();

  /// Creates a register allocator as the user specified on the command line, or
  /// picks one that matches OptLevel.
  ///
  FunctionPass *createRegisterAllocator(CodeGenOpt::Level OptLevel);

  /// FastRegisterAllocation Pass - This pass register allocates as fast as
  /// possible. It is best suited for debug code where live ranges are short.
  ///
  FunctionPass *createFastRegisterAllocator();

  /// BasicRegisterAllocation Pass - This pass implements a degenerate global
  /// register allocator using the basic regalloc framework.
  ///
  FunctionPass *createBasicRegisterAllocator();

  /// Greedy register allocation pass - This pass implements a global register
  /// allocator for optimized builds.
  ///
  FunctionPass *createGreedyRegisterAllocator();

  /// LinearScanRegisterAllocation Pass - This pass implements the linear scan
  /// register allocation algorithm, a global register allocator.
  ///
  FunctionPass *createLinearScanRegisterAllocator();

  /// PBQPRegisterAllocation Pass - This pass implements the Partitioned Boolean
  /// Quadratic Prograaming (PBQP) based register allocator.
  ///
  FunctionPass *createDefaultPBQPRegisterAllocator();

  /// PrologEpilogCodeInserter Pass - This pass inserts prolog and epilog code,
  /// and eliminates abstract frame references.
  ///
  FunctionPass *createPrologEpilogCodeInserter();

  /// ExpandPostRAPseudos Pass - This pass expands pseudo instructions after
  /// register allocation.
  ///
  FunctionPass *createExpandPostRAPseudosPass();

  /// createPostRAScheduler - This pass performs post register allocation
  /// scheduling.
  FunctionPass *createPostRAScheduler(CodeGenOpt::Level OptLevel);

  /// BranchFolding Pass - This pass performs machine code CFG based
  /// optimizations to delete branches to branches, eliminate branches to
  /// successor blocks (creating fall throughs), and eliminating branches over
  /// branches.
  FunctionPass *createBranchFoldingPass(bool DefaultEnableTailMerge);

  /// TailDuplicate Pass - Duplicate blocks with unconditional branches
  /// into tails of their predecessors.
  FunctionPass *createTailDuplicatePass(bool PreRegAlloc = false);

  /// IfConverter Pass - This pass performs machine code if conversion.
  FunctionPass *createIfConverterPass();

  /// Code Placement Pass - This pass optimize code placement and aligns loop
  /// headers to target specific alignment boundary.
  FunctionPass *createCodePlacementOptPass();

  /// IntrinsicLowering Pass - Performs target-independent LLVM IR
  /// transformations for highly portable strategies.
  FunctionPass *createGCLoweringPass();

  /// MachineCodeAnalysis Pass - Target-independent pass to mark safe points in
  /// machine code. Must be added very late during code generation, just prior
  /// to output, and importantly after all CFG transformations (such as branch
  /// folding).
  FunctionPass *createGCMachineCodeAnalysisPass();

  /// Deleter Pass - Releases GC metadata.
  ///
  FunctionPass *createGCInfoDeleter();

  /// Creates a pass to print GC metadata.
  ///
  FunctionPass *createGCInfoPrinter(raw_ostream &OS);

  /// createMachineCSEPass - This pass performs global CSE on machine
  /// instructions.
  FunctionPass *createMachineCSEPass();

  /// createMachineLICMPass - This pass performs LICM on machine instructions.
  ///
  FunctionPass *createMachineLICMPass(bool PreRegAlloc = true);

  /// createMachineSinkingPass - This pass performs sinking on machine
  /// instructions.
  FunctionPass *createMachineSinkingPass();

  /// createPeepholeOptimizerPass - This pass performs peephole optimizations -
  /// like extension and comparison eliminations.
  FunctionPass *createPeepholeOptimizerPass();

  /// createOptimizePHIsPass - This pass optimizes machine instruction PHIs
  /// to take advantage of opportunities created during DAG legalization.
  FunctionPass *createOptimizePHIsPass();

  /// createStackSlotColoringPass - This pass performs stack slot coloring.
  FunctionPass *createStackSlotColoringPass(bool);

  /// createStackProtectorPass - This pass adds stack protectors to functions.
  FunctionPass *createStackProtectorPass(const TargetLowering *tli);

  /// createMachineVerifierPass - This pass verifies cenerated machine code
  /// instructions for correctness.
  FunctionPass *createMachineVerifierPass(const char *Banner = 0);

  /// createDwarfEHPass - This pass mulches exception handling code into a form
  /// adapted to code generation.  Required if using dwarf exception handling.
  FunctionPass *createDwarfEHPass(const TargetMachine *tm);

  /// createSjLjEHPass - This pass adapts exception handling code to use
  /// the GCC-style builtin setjmp/longjmp (sjlj) to handling EH control flow.
  FunctionPass *createSjLjEHPass(const TargetLowering *tli);

  /// createLocalStackSlotAllocationPass - This pass assigns local frame
  /// indices to stack slots relative to one another and allocates
  /// base registers to access them when it is estimated by the target to
  /// be out of range of normal frame pointer or stack pointer index
  /// addressing.
  FunctionPass *createLocalStackSlotAllocationPass();

  /// createExpandISelPseudosPass - This pass expands pseudo-instructions.
  ///
  FunctionPass *createExpandISelPseudosPass();

  /// createExecutionDependencyFixPass - This pass fixes execution time
  /// problems with dependent instructions, such as switching execution
  /// domains to match.
  ///
  /// The pass will examine instructions using and defining registers in RC.
  ///
  FunctionPass *createExecutionDependencyFixPass(const TargetRegisterClass *RC);

} // End llvm namespace

#endif
