//===-- llvm/Instruction.h - Instruction class definition -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the Instruction class, which is the
// base class for all of the LLVM instructions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_INSTRUCTION_H
#define LLVM_INSTRUCTION_H

#include "llvm/User.h"
#include "llvm/ADT/ilist_node.h"
#include "llvm/Support/DebugLoc.h"

namespace llvm {

class LLVMContext;
class MDNode;

template<typename ValueSubClass, typename ItemParentClass>
  class SymbolTableListTraits;

class Instruction : public User, public ilist_node<Instruction> {
  void operator=(const Instruction &);     // Do not implement
  Instruction(const Instruction &);        // Do not implement

  BasicBlock *Parent;
  DebugLoc DbgLoc;                         // 'dbg' Metadata cache.
  
  enum {
    /// HasMetadataBit - This is a bit stored in the SubClassData field which
    /// indicates whether this instruction has metadata attached to it or not.
    HasMetadataBit = 1 << 15
  };
public:
  // Out of line virtual method, so the vtable, etc has a home.
  ~Instruction();
  
  /// use_back - Specialize the methods defined in Value, as we know that an
  /// instruction can only be used by other instructions.
  Instruction       *use_back()       { return cast<Instruction>(*use_begin());}
  const Instruction *use_back() const { return cast<Instruction>(*use_begin());}
  
  inline const BasicBlock *getParent() const { return Parent; }
  inline       BasicBlock *getParent()       { return Parent; }

  /// removeFromParent - This method unlinks 'this' from the containing basic
  /// block, but does not delete it.
  ///
  void removeFromParent();

  /// eraseFromParent - This method unlinks 'this' from the containing basic
  /// block and deletes it.
  ///
  void eraseFromParent();

  /// insertBefore - Insert an unlinked instructions into a basic block
  /// immediately before the specified instruction.
  void insertBefore(Instruction *InsertPos);

  /// insertAfter - Insert an unlinked instructions into a basic block
  /// immediately after the specified instruction.
  void insertAfter(Instruction *InsertPos);

  /// moveBefore - Unlink this instruction from its current basic block and
  /// insert it into the basic block that MovePos lives in, right before
  /// MovePos.
  void moveBefore(Instruction *MovePos);

  //===--------------------------------------------------------------------===//
  // Subclass classification.
  //===--------------------------------------------------------------------===//
  
  /// getOpcode() returns a member of one of the enums like Instruction::Add.
  unsigned getOpcode() const { return getValueID() - InstructionVal; }
  
  const char *getOpcodeName() const { return getOpcodeName(getOpcode()); }
  bool isTerminator() const { return isTerminator(getOpcode()); }
  bool isBinaryOp() const { return isBinaryOp(getOpcode()); }
  bool isShift() { return isShift(getOpcode()); }
  bool isCast() const { return isCast(getOpcode()); }
  
  static const char* getOpcodeName(unsigned OpCode);

  static inline bool isTerminator(unsigned OpCode) {
    return OpCode >= TermOpsBegin && OpCode < TermOpsEnd;
  }

  static inline bool isBinaryOp(unsigned Opcode) {
    return Opcode >= BinaryOpsBegin && Opcode < BinaryOpsEnd;
  }

  /// @brief Determine if the Opcode is one of the shift instructions.
  static inline bool isShift(unsigned Opcode) {
    return Opcode >= Shl && Opcode <= AShr;
  }

  /// isLogicalShift - Return true if this is a logical shift left or a logical
  /// shift right.
  inline bool isLogicalShift() const {
    return getOpcode() == Shl || getOpcode() == LShr;
  }

  /// isArithmeticShift - Return true if this is an arithmetic shift right.
  inline bool isArithmeticShift() const {
    return getOpcode() == AShr;
  }

  /// @brief Determine if the OpCode is one of the CastInst instructions.
  static inline bool isCast(unsigned OpCode) {
    return OpCode >= CastOpsBegin && OpCode < CastOpsEnd;
  }

  //===--------------------------------------------------------------------===//
  // Metadata manipulation.
  //===--------------------------------------------------------------------===//
  
  /// hasMetadata() - Return true if this instruction has any metadata attached
  /// to it.
  bool hasMetadata() const {
    return !DbgLoc.isUnknown() || hasMetadataHashEntry();
  }
  
  /// hasMetadataOtherThanDebugLoc - Return true if this instruction has
  /// metadata attached to it other than a debug location.
  bool hasMetadataOtherThanDebugLoc() const {
    return hasMetadataHashEntry();
  }
  
  /// getMetadata - Get the metadata of given kind attached to this Instruction.
  /// If the metadata is not found then return null.
  MDNode *getMetadata(unsigned KindID) const {
    if (!hasMetadata()) return 0;
    return getMetadataImpl(KindID);
  }
  
  /// getMetadata - Get the metadata of given kind attached to this Instruction.
  /// If the metadata is not found then return null.
  MDNode *getMetadata(const char *Kind) const {
    if (!hasMetadata()) return 0;
    return getMetadataImpl(Kind);
  }
  
  /// getAllMetadata - Get all metadata attached to this Instruction.  The first
  /// element of each pair returned is the KindID, the second element is the
  /// metadata value.  This list is returned sorted by the KindID.
  void getAllMetadata(SmallVectorImpl<std::pair<unsigned, MDNode*> > &MDs)const{
    if (hasMetadata())
      getAllMetadataImpl(MDs);
  }
  
  /// getAllMetadataOtherThanDebugLoc - This does the same thing as
  /// getAllMetadata, except that it filters out the debug location.
  void getAllMetadataOtherThanDebugLoc(SmallVectorImpl<std::pair<unsigned,
                                       MDNode*> > &MDs) const {
    if (hasMetadataOtherThanDebugLoc())
      getAllMetadataOtherThanDebugLocImpl(MDs);
  }
  
  /// setMetadata - Set the metadata of the specified kind to the specified
  /// node.  This updates/replaces metadata if already present, or removes it if
  /// Node is null.
  void setMetadata(unsigned KindID, MDNode *Node);
  void setMetadata(const char *Kind, MDNode *Node);

  /// setDebugLoc - Set the debug location information for this instruction.
  void setDebugLoc(const DebugLoc &Loc) { DbgLoc = Loc; }
  
  /// getDebugLoc - Return the debug location for this node as a DebugLoc.
  const DebugLoc &getDebugLoc() const { return DbgLoc; }
  
private:
  /// hasMetadataHashEntry - Return true if we have an entry in the on-the-side
  /// metadata hash.
  bool hasMetadataHashEntry() const {
    return (getSubclassDataFromValue() & HasMetadataBit) != 0;
  }
  
  // These are all implemented in Metadata.cpp.
  MDNode *getMetadataImpl(unsigned KindID) const;
  MDNode *getMetadataImpl(const char *Kind) const;
  void getAllMetadataImpl(SmallVectorImpl<std::pair<unsigned,MDNode*> > &)const;
  void getAllMetadataOtherThanDebugLocImpl(SmallVectorImpl<std::pair<unsigned,
                                           MDNode*> > &) const;
  void clearMetadataHashEntries();
public:
  //===--------------------------------------------------------------------===//
  // Predicates and helper methods.
  //===--------------------------------------------------------------------===//
  
  
  /// isAssociative - Return true if the instruction is associative:
  ///
  ///   Associative operators satisfy:  x op (y op z) === (x op y) op z
  ///
  /// In LLVM, the Add, Mul, And, Or, and Xor operators are associative.
  ///
  bool isAssociative() const { return isAssociative(getOpcode()); }
  static bool isAssociative(unsigned op);

  /// isCommutative - Return true if the instruction is commutative:
  ///
  ///   Commutative operators satisfy: (x op y) === (y op x)
  ///
  /// In LLVM, these are the associative operators, plus SetEQ and SetNE, when
  /// applied to any type.
  ///
  bool isCommutative() const { return isCommutative(getOpcode()); }
  static bool isCommutative(unsigned op);

  /// mayWriteToMemory - Return true if this instruction may modify memory.
  ///
  bool mayWriteToMemory() const;

  /// mayReadFromMemory - Return true if this instruction may read memory.
  ///
  bool mayReadFromMemory() const;

  /// mayReadOrWriteMemory - Return true if this instruction may read or
  /// write memory.
  ///
  bool mayReadOrWriteMemory() const {
    return mayReadFromMemory() || mayWriteToMemory();
  }

  /// mayThrow - Return true if this instruction may throw an exception.
  ///
  bool mayThrow() const;

  /// mayHaveSideEffects - Return true if the instruction may have side effects.
  ///
  /// Note that this does not consider malloc and alloca to have side
  /// effects because the newly allocated memory is completely invisible to
  /// instructions which don't used the returned value.  For cases where this
  /// matters, isSafeToSpeculativelyExecute may be more appropriate.
  bool mayHaveSideEffects() const {
    return mayWriteToMemory() || mayThrow();
  }

  /// isSafeToSpeculativelyExecute - Return true if the instruction does not
  /// have any effects besides calculating the result and does not have
  /// undefined behavior.
  ///
  /// This method never returns true for an instruction that returns true for
  /// mayHaveSideEffects; however, this method also does some other checks in
  /// addition. It checks for undefined behavior, like dividing by zero or
  /// loading from an invalid pointer (but not for undefined results, like a
  /// shift with a shift amount larger than the width of the result). It checks
  /// for malloc and alloca because speculatively executing them might cause a
  /// memory leak. It also returns false for instructions related to control
  /// flow, specifically terminators and PHI nodes.
  ///
  /// This method only looks at the instruction itself and its operands, so if
  /// this method returns true, it is safe to move the instruction as long as
  /// the correct dominance relationships for the operands and users hold.
  /// However, this method can return true for instructions that read memory;
  /// for such instructions, moving them may change the resulting value.
  bool isSafeToSpeculativelyExecute() const;

  /// clone() - Create a copy of 'this' instruction that is identical in all
  /// ways except the following:
  ///   * The instruction has no parent
  ///   * The instruction has no name
  ///
  Instruction *clone() const;
  
  /// isIdenticalTo - Return true if the specified instruction is exactly
  /// identical to the current one.  This means that all operands match and any
  /// extra information (e.g. load is volatile) agree.
  bool isIdenticalTo(const Instruction *I) const;
  
  /// isIdenticalToWhenDefined - This is like isIdenticalTo, except that it
  /// ignores the SubclassOptionalData flags, which specify conditions
  /// under which the instruction's result is undefined.
  bool isIdenticalToWhenDefined(const Instruction *I) const;
  
  /// This function determines if the specified instruction executes the same
  /// operation as the current one. This means that the opcodes, type, operand
  /// types and any other factors affecting the operation must be the same. This
  /// is similar to isIdenticalTo except the operands themselves don't have to
  /// be identical.
  /// @returns true if the specified instruction is the same operation as
  /// the current one.
  /// @brief Determine if one instruction is the same operation as another.
  bool isSameOperationAs(const Instruction *I) const;
  
  /// isUsedOutsideOfBlock - Return true if there are any uses of this
  /// instruction in blocks other than the specified block.  Note that PHI nodes
  /// are considered to evaluate their operands in the corresponding predecessor
  /// block.
  bool isUsedOutsideOfBlock(const BasicBlock *BB) const;
  
  
  /// Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const Instruction *) { return true; }
  static inline bool classof(const Value *V) {
    return V->getValueID() >= Value::InstructionVal;
  }

  //----------------------------------------------------------------------
  // Exported enumerations.
  //
  enum TermOps {       // These terminate basic blocks
#define  FIRST_TERM_INST(N)             TermOpsBegin = N,
#define HANDLE_TERM_INST(N, OPC, CLASS) OPC = N,
#define   LAST_TERM_INST(N)             TermOpsEnd = N+1
#include "llvm/Instruction.def"
  };

  enum BinaryOps {
#define  FIRST_BINARY_INST(N)             BinaryOpsBegin = N,
#define HANDLE_BINARY_INST(N, OPC, CLASS) OPC = N,
#define   LAST_BINARY_INST(N)             BinaryOpsEnd = N+1
#include "llvm/Instruction.def"
  };

  enum MemoryOps {
#define  FIRST_MEMORY_INST(N)             MemoryOpsBegin = N,
#define HANDLE_MEMORY_INST(N, OPC, CLASS) OPC = N,
#define   LAST_MEMORY_INST(N)             MemoryOpsEnd = N+1
#include "llvm/Instruction.def"
  };

  enum CastOps {
#define  FIRST_CAST_INST(N)             CastOpsBegin = N,
#define HANDLE_CAST_INST(N, OPC, CLASS) OPC = N,
#define   LAST_CAST_INST(N)             CastOpsEnd = N+1
#include "llvm/Instruction.def"
  };

  enum OtherOps {
#define  FIRST_OTHER_INST(N)             OtherOpsBegin = N,
#define HANDLE_OTHER_INST(N, OPC, CLASS) OPC = N,
#define   LAST_OTHER_INST(N)             OtherOpsEnd = N+1
#include "llvm/Instruction.def"
  };
private:
  // Shadow Value::setValueSubclassData with a private forwarding method so that
  // subclasses cannot accidentally use it.
  void setValueSubclassData(unsigned short D) {
    Value::setValueSubclassData(D);
  }
  unsigned short getSubclassDataFromValue() const {
    return Value::getSubclassDataFromValue();
  }
  
  void setHasMetadataHashEntry(bool V) {
    setValueSubclassData((getSubclassDataFromValue() & ~HasMetadataBit) |
                         (V ? HasMetadataBit : 0));
  }
  
  friend class SymbolTableListTraits<Instruction, BasicBlock>;
  void setParent(BasicBlock *P);
protected:
  // Instruction subclasses can stick up to 15 bits of stuff into the
  // SubclassData field of instruction with these members.
  
  // Verify that only the low 15 bits are used.
  void setInstructionSubclassData(unsigned short D) {
    assert((D & HasMetadataBit) == 0 && "Out of range value put into field");
    setValueSubclassData((getSubclassDataFromValue() & HasMetadataBit) | D);
  }
  
  unsigned getSubclassDataFromInstruction() const {
    return getSubclassDataFromValue() & ~HasMetadataBit;
  }
  
  Instruction(Type *Ty, unsigned iType, Use *Ops, unsigned NumOps,
              Instruction *InsertBefore = 0);
  Instruction(Type *Ty, unsigned iType, Use *Ops, unsigned NumOps,
              BasicBlock *InsertAtEnd);
  virtual Instruction *clone_impl() const = 0;
  
};

// Instruction* is only 4-byte aligned.
template<>
class PointerLikeTypeTraits<Instruction*> {
  typedef Instruction* PT;
public:
  static inline void *getAsVoidPointer(PT P) { return P; }
  static inline PT getFromVoidPointer(void *P) {
    return static_cast<PT>(P);
  }
  enum { NumLowBitsAvailable = 2 };
};
  
} // End llvm namespace

#endif
