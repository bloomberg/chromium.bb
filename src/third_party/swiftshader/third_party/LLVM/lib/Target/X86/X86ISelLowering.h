//===-- X86ISelLowering.h - X86 DAG Lowering Interface ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that X86 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef X86ISELLOWERING_H
#define X86ISELLOWERING_H

#include "X86Subtarget.h"
#include "X86RegisterInfo.h"
#include "X86MachineFunctionInfo.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/CodeGen/FastISel.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/CallingConvLower.h"

namespace llvm {
  namespace X86ISD {
    // X86 Specific DAG Nodes
    enum NodeType {
      // Start the numbering where the builtin ops leave off.
      FIRST_NUMBER = ISD::BUILTIN_OP_END,

      /// BSF - Bit scan forward.
      /// BSR - Bit scan reverse.
      BSF,
      BSR,

      /// SHLD, SHRD - Double shift instructions. These correspond to
      /// X86::SHLDxx and X86::SHRDxx instructions.
      SHLD,
      SHRD,

      /// FAND - Bitwise logical AND of floating point values. This corresponds
      /// to X86::ANDPS or X86::ANDPD.
      FAND,

      /// FOR - Bitwise logical OR of floating point values. This corresponds
      /// to X86::ORPS or X86::ORPD.
      FOR,

      /// FXOR - Bitwise logical XOR of floating point values. This corresponds
      /// to X86::XORPS or X86::XORPD.
      FXOR,

      /// FSRL - Bitwise logical right shift of floating point values. These
      /// corresponds to X86::PSRLDQ.
      FSRL,

      /// CALL - These operations represent an abstract X86 call
      /// instruction, which includes a bunch of information.  In particular the
      /// operands of these node are:
      ///
      ///     #0 - The incoming token chain
      ///     #1 - The callee
      ///     #2 - The number of arg bytes the caller pushes on the stack.
      ///     #3 - The number of arg bytes the callee pops off the stack.
      ///     #4 - The value to pass in AL/AX/EAX (optional)
      ///     #5 - The value to pass in DL/DX/EDX (optional)
      ///
      /// The result values of these nodes are:
      ///
      ///     #0 - The outgoing token chain
      ///     #1 - The first register result value (optional)
      ///     #2 - The second register result value (optional)
      ///
      CALL,

      /// RDTSC_DAG - This operation implements the lowering for
      /// readcyclecounter
      RDTSC_DAG,

      /// X86 compare and logical compare instructions.
      CMP, COMI, UCOMI,

      /// X86 bit-test instructions.
      BT,

      /// X86 SetCC. Operand 0 is condition code, and operand 1 is the EFLAGS
      /// operand, usually produced by a CMP instruction.
      SETCC,

      // Same as SETCC except it's materialized with a sbb and the value is all
      // one's or all zero's.
      SETCC_CARRY,  // R = carry_bit ? ~0 : 0

      /// X86 FP SETCC, implemented with CMP{cc}SS/CMP{cc}SD.
      /// Operands are two FP values to compare; result is a mask of
      /// 0s or 1s.  Generally DTRT for C/C++ with NaNs.
      FSETCCss, FSETCCsd,

      /// X86 MOVMSK{pd|ps}, extracts sign bits of two or four FP values,
      /// result in an integer GPR.  Needs masking for scalar result.
      FGETSIGNx86,

      /// X86 conditional moves. Operand 0 and operand 1 are the two values
      /// to select from. Operand 2 is the condition code, and operand 3 is the
      /// flag operand produced by a CMP or TEST instruction. It also writes a
      /// flag result.
      CMOV,

      /// X86 conditional branches. Operand 0 is the chain operand, operand 1
      /// is the block to branch if condition is true, operand 2 is the
      /// condition code, and operand 3 is the flag operand produced by a CMP
      /// or TEST instruction.
      BRCOND,

      /// Return with a flag operand. Operand 0 is the chain operand, operand
      /// 1 is the number of bytes of stack to pop.
      RET_FLAG,

      /// REP_STOS - Repeat fill, corresponds to X86::REP_STOSx.
      REP_STOS,

      /// REP_MOVS - Repeat move, corresponds to X86::REP_MOVSx.
      REP_MOVS,

      /// GlobalBaseReg - On Darwin, this node represents the result of the popl
      /// at function entry, used for PIC code.
      GlobalBaseReg,

      /// Wrapper - A wrapper node for TargetConstantPool,
      /// TargetExternalSymbol, and TargetGlobalAddress.
      Wrapper,

      /// WrapperRIP - Special wrapper used under X86-64 PIC mode for RIP
      /// relative displacements.
      WrapperRIP,

      /// MOVQ2DQ - Copies a 64-bit value from an MMX vector to the low word
      /// of an XMM vector, with the high word zero filled.
      MOVQ2DQ,

      /// MOVDQ2Q - Copies a 64-bit value from the low word of an XMM vector
      /// to an MMX vector.  If you think this is too close to the previous
      /// mnemonic, so do I; blame Intel.
      MOVDQ2Q,

      /// vector to a GPR.
      MMX_MOVD2W,
 
      /// MMX_MOVW2D - Copies a GPR into the low 32-bit word of a MMX vector
      /// and zero out the high word.
      MMX_MOVW2D,

      /// PEXTRB - Extract an 8-bit value from a vector and zero extend it to
      /// i32, corresponds to X86::PEXTRB.
      PEXTRB,

      /// PEXTRW - Extract a 16-bit value from a vector and zero extend it to
      /// i32, corresponds to X86::PEXTRW.
      PEXTRW,

      /// INSERTPS - Insert any element of a 4 x float vector into any element
      /// of a destination 4 x floatvector.
      INSERTPS,

      /// PINSRB - Insert the lower 8-bits of a 32-bit value to a vector,
      /// corresponds to X86::PINSRB.
      PINSRB,

      /// PINSRW - Insert the lower 16-bits of a 32-bit value to a vector,
      /// corresponds to X86::PINSRW.
      PINSRW, MMX_PINSRW,

      /// PSHUFB - Shuffle 16 8-bit values within a vector.
      PSHUFB,

      /// ANDNP - Bitwise Logical AND NOT of Packed FP values.
      ANDNP,

      /// PSIGNB/W/D - Copy integer sign.
      PSIGNB, PSIGNW, PSIGND,

      /// BLEND family of opcodes
      BLENDV,

      /// FHADD - Floating point horizontal add.
      FHADD,

      /// FHSUB - Floating point horizontal sub.
      FHSUB,

      /// FMAX, FMIN - Floating point max and min.
      ///
      FMAX, FMIN,

      /// FRSQRT, FRCP - Floating point reciprocal-sqrt and reciprocal
      /// approximation.  Note that these typically require refinement
      /// in order to obtain suitable precision.
      FRSQRT, FRCP,

      // TLSADDR - Thread Local Storage.
      TLSADDR,

      // TLSCALL - Thread Local Storage.  When calling to an OS provided
      // thunk at the address from an earlier relocation.
      TLSCALL,

      // EH_RETURN - Exception Handling helpers.
      EH_RETURN,

      /// TC_RETURN - Tail call return.
      ///   operand #0 chain
      ///   operand #1 callee (register or absolute)
      ///   operand #2 stack adjustment
      ///   operand #3 optional in flag
      TC_RETURN,

      // VZEXT_MOVL - Vector move low and zero extend.
      VZEXT_MOVL,

      // VSHL, VSRL - Vector logical left / right shift.
      VSHL, VSRL,

      // CMPPD, CMPPS - Vector double/float comparison.
      // CMPPD, CMPPS - Vector double/float comparison.
      CMPPD, CMPPS,

      // PCMP* - Vector integer comparisons.
      PCMPEQB, PCMPEQW, PCMPEQD, PCMPEQQ,
      PCMPGTB, PCMPGTW, PCMPGTD, PCMPGTQ,

      // ADD, SUB, SMUL, etc. - Arithmetic operations with FLAGS results.
      ADD, SUB, ADC, SBB, SMUL,
      INC, DEC, OR, XOR, AND,

      ANDN, // ANDN - Bitwise AND NOT with FLAGS results.

      UMUL, // LOW, HI, FLAGS = umul LHS, RHS

      // MUL_IMM - X86 specific multiply by immediate.
      MUL_IMM,

      // PTEST - Vector bitwise comparisons
      PTEST,

      // TESTP - Vector packed fp sign bitwise comparisons
      TESTP,

      // Several flavors of instructions with vector shuffle behaviors.
      PALIGN,
      PSHUFD,
      PSHUFHW,
      PSHUFLW,
      PSHUFHW_LD,
      PSHUFLW_LD,
      SHUFPD,
      SHUFPS,
      MOVDDUP,
      MOVSHDUP,
      MOVSLDUP,
      MOVSHDUP_LD,
      MOVSLDUP_LD,
      MOVLHPS,
      MOVLHPD,
      MOVHLPS,
      MOVHLPD,
      MOVLPS,
      MOVLPD,
      MOVSD,
      MOVSS,
      UNPCKLPS,
      UNPCKLPD,
      VUNPCKLPSY,
      VUNPCKLPDY,
      UNPCKHPS,
      UNPCKHPD,
      VUNPCKHPSY,
      VUNPCKHPDY,
      PUNPCKLBW,
      PUNPCKLWD,
      PUNPCKLDQ,
      PUNPCKLQDQ,
      PUNPCKHBW,
      PUNPCKHWD,
      PUNPCKHDQ,
      PUNPCKHQDQ,
      VPERMILPS,
      VPERMILPSY,
      VPERMILPD,
      VPERMILPDY,
      VPERM2F128,
      VBROADCAST,

      // VASTART_SAVE_XMM_REGS - Save xmm argument registers to the stack,
      // according to %al. An operator is needed so that this can be expanded
      // with control flow.
      VASTART_SAVE_XMM_REGS,

      // WIN_ALLOCA - Windows's _chkstk call to do stack probing.
      WIN_ALLOCA,

      // SEG_ALLOCA - For allocating variable amounts of stack space when using
      // segmented stacks. Check if the current stacklet has enough space, and
      // falls back to heap allocation if not.
      SEG_ALLOCA,

      // Memory barrier
      MEMBARRIER,
      MFENCE,
      SFENCE,
      LFENCE,

      // ATOMADD64_DAG, ATOMSUB64_DAG, ATOMOR64_DAG, ATOMAND64_DAG,
      // ATOMXOR64_DAG, ATOMNAND64_DAG, ATOMSWAP64_DAG -
      // Atomic 64-bit binary operations.
      ATOMADD64_DAG = ISD::FIRST_TARGET_MEMORY_OPCODE,
      ATOMSUB64_DAG,
      ATOMOR64_DAG,
      ATOMXOR64_DAG,
      ATOMAND64_DAG,
      ATOMNAND64_DAG,
      ATOMSWAP64_DAG,

      // LCMPXCHG_DAG, LCMPXCHG8_DAG, LCMPXCHG16_DAG - Compare and swap.
      LCMPXCHG_DAG,
      LCMPXCHG8_DAG,
      LCMPXCHG16_DAG,

      // VZEXT_LOAD - Load, scalar_to_vector, and zero extend.
      VZEXT_LOAD,

      // FNSTCW16m - Store FP control world into i16 memory.
      FNSTCW16m,

      /// FP_TO_INT*_IN_MEM - This instruction implements FP_TO_SINT with the
      /// integer destination in memory and a FP reg source.  This corresponds
      /// to the X86::FIST*m instructions and the rounding mode change stuff. It
      /// has two inputs (token chain and address) and two outputs (int value
      /// and token chain).
      FP_TO_INT16_IN_MEM,
      FP_TO_INT32_IN_MEM,
      FP_TO_INT64_IN_MEM,

      /// FILD, FILD_FLAG - This instruction implements SINT_TO_FP with the
      /// integer source in memory and FP reg result.  This corresponds to the
      /// X86::FILD*m instructions. It has three inputs (token chain, address,
      /// and source type) and two outputs (FP value and token chain). FILD_FLAG
      /// also produces a flag).
      FILD,
      FILD_FLAG,

      /// FLD - This instruction implements an extending load to FP stack slots.
      /// This corresponds to the X86::FLD32m / X86::FLD64m. It takes a chain
      /// operand, ptr to load from, and a ValueType node indicating the type
      /// to load to.
      FLD,

      /// FST - This instruction implements a truncating store to FP stack
      /// slots. This corresponds to the X86::FST32m / X86::FST64m. It takes a
      /// chain operand, value to store, address, and a ValueType to store it
      /// as.
      FST,

      /// VAARG_64 - This instruction grabs the address of the next argument
      /// from a va_list. (reads and modifies the va_list in memory)
      VAARG_64

      // WARNING: Do not add anything in the end unless you want the node to
      // have memop! In fact, starting from ATOMADD64_DAG all opcodes will be
      // thought as target memory ops!
    };
  }

  /// Define some predicates that are used for node matching.
  namespace X86 {
    /// isPSHUFDMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to PSHUFD.
    bool isPSHUFDMask(ShuffleVectorSDNode *N);

    /// isPSHUFHWMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to PSHUFD.
    bool isPSHUFHWMask(ShuffleVectorSDNode *N);

    /// isPSHUFLWMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to PSHUFD.
    bool isPSHUFLWMask(ShuffleVectorSDNode *N);

    /// isSHUFPMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to SHUFP*.
    bool isSHUFPMask(ShuffleVectorSDNode *N);

    /// isMOVHLPSMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to MOVHLPS.
    bool isMOVHLPSMask(ShuffleVectorSDNode *N);

    /// isMOVHLPS_v_undef_Mask - Special case of isMOVHLPSMask for canonical form
    /// of vector_shuffle v, v, <2, 3, 2, 3>, i.e. vector_shuffle v, undef,
    /// <2, 3, 2, 3>
    bool isMOVHLPS_v_undef_Mask(ShuffleVectorSDNode *N);

    /// isMOVLPMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for MOVLP{S|D}.
    bool isMOVLPMask(ShuffleVectorSDNode *N);

    /// isMOVHPMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for MOVHP{S|D}.
    /// as well as MOVLHPS.
    bool isMOVLHPSMask(ShuffleVectorSDNode *N);

    /// isUNPCKLMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to UNPCKL.
    bool isUNPCKLMask(ShuffleVectorSDNode *N, bool V2IsSplat = false);

    /// isUNPCKHMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to UNPCKH.
    bool isUNPCKHMask(ShuffleVectorSDNode *N, bool V2IsSplat = false);

    /// isUNPCKL_v_undef_Mask - Special case of isUNPCKLMask for canonical form
    /// of vector_shuffle v, v, <0, 4, 1, 5>, i.e. vector_shuffle v, undef,
    /// <0, 0, 1, 1>
    bool isUNPCKL_v_undef_Mask(ShuffleVectorSDNode *N);

    /// isUNPCKH_v_undef_Mask - Special case of isUNPCKHMask for canonical form
    /// of vector_shuffle v, v, <2, 6, 3, 7>, i.e. vector_shuffle v, undef,
    /// <2, 2, 3, 3>
    bool isUNPCKH_v_undef_Mask(ShuffleVectorSDNode *N);

    /// isMOVLMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to MOVSS,
    /// MOVSD, and MOVD, i.e. setting the lowest element.
    bool isMOVLMask(ShuffleVectorSDNode *N);

    /// isMOVSHDUPMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to MOVSHDUP.
    bool isMOVSHDUPMask(ShuffleVectorSDNode *N, const X86Subtarget *Subtarget);

    /// isMOVSLDUPMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to MOVSLDUP.
    bool isMOVSLDUPMask(ShuffleVectorSDNode *N, const X86Subtarget *Subtarget);

    /// isMOVDDUPMask - Return true if the specified VECTOR_SHUFFLE operand
    /// specifies a shuffle of elements that is suitable for input to MOVDDUP.
    bool isMOVDDUPMask(ShuffleVectorSDNode *N);

    /// isVEXTRACTF128Index - Return true if the specified
    /// EXTRACT_SUBVECTOR operand specifies a vector extract that is
    /// suitable for input to VEXTRACTF128.
    bool isVEXTRACTF128Index(SDNode *N);

    /// isVINSERTF128Index - Return true if the specified
    /// INSERT_SUBVECTOR operand specifies a subvector insert that is
    /// suitable for input to VINSERTF128.
    bool isVINSERTF128Index(SDNode *N);

    /// getShuffleSHUFImmediate - Return the appropriate immediate to shuffle
    /// the specified isShuffleMask VECTOR_SHUFFLE mask with PSHUF* and SHUFP*
    /// instructions.
    unsigned getShuffleSHUFImmediate(SDNode *N);

    /// getShufflePSHUFHWImmediate - Return the appropriate immediate to shuffle
    /// the specified VECTOR_SHUFFLE mask with PSHUFHW instruction.
    unsigned getShufflePSHUFHWImmediate(SDNode *N);

    /// getShufflePSHUFLWImmediate - Return the appropriate immediate to shuffle
    /// the specified VECTOR_SHUFFLE mask with PSHUFLW instruction.
    unsigned getShufflePSHUFLWImmediate(SDNode *N);

    /// getShufflePALIGNRImmediate - Return the appropriate immediate to shuffle
    /// the specified VECTOR_SHUFFLE mask with the PALIGNR instruction.
    unsigned getShufflePALIGNRImmediate(SDNode *N);

    /// getExtractVEXTRACTF128Immediate - Return the appropriate
    /// immediate to extract the specified EXTRACT_SUBVECTOR index
    /// with VEXTRACTF128 instructions.
    unsigned getExtractVEXTRACTF128Immediate(SDNode *N);

    /// getInsertVINSERTF128Immediate - Return the appropriate
    /// immediate to insert at the specified INSERT_SUBVECTOR index
    /// with VINSERTF128 instructions.
    unsigned getInsertVINSERTF128Immediate(SDNode *N);

    /// isZeroNode - Returns true if Elt is a constant zero or a floating point
    /// constant +0.0.
    bool isZeroNode(SDValue Elt);

    /// isOffsetSuitableForCodeModel - Returns true of the given offset can be
    /// fit into displacement field of the instruction.
    bool isOffsetSuitableForCodeModel(int64_t Offset, CodeModel::Model M,
                                      bool hasSymbolicDisplacement = true);


    /// isCalleePop - Determines whether the callee is required to pop its
    /// own arguments. Callee pop is necessary to support tail calls.
    bool isCalleePop(CallingConv::ID CallingConv,
                     bool is64Bit, bool IsVarArg, bool TailCallOpt);
  }

  //===--------------------------------------------------------------------===//
  //  X86TargetLowering - X86 Implementation of the TargetLowering interface
  class X86TargetLowering : public TargetLowering {
  public:
    explicit X86TargetLowering(X86TargetMachine &TM);

    virtual unsigned getJumpTableEncoding() const;

    virtual MVT getShiftAmountTy(EVT LHSTy) const { return MVT::i8; }

    virtual const MCExpr *
    LowerCustomJumpTableEntry(const MachineJumpTableInfo *MJTI,
                              const MachineBasicBlock *MBB, unsigned uid,
                              MCContext &Ctx) const;

    /// getPICJumpTableRelocaBase - Returns relocation base for the given PIC
    /// jumptable.
    virtual SDValue getPICJumpTableRelocBase(SDValue Table,
                                             SelectionDAG &DAG) const;
    virtual const MCExpr *
    getPICJumpTableRelocBaseExpr(const MachineFunction *MF,
                                 unsigned JTI, MCContext &Ctx) const;

    /// getStackPtrReg - Return the stack pointer register we are using: either
    /// ESP or RSP.
    unsigned getStackPtrReg() const { return X86StackPtr; }

    /// getByValTypeAlignment - Return the desired alignment for ByVal aggregate
    /// function arguments in the caller parameter area. For X86, aggregates
    /// that contains are placed at 16-byte boundaries while the rest are at
    /// 4-byte boundaries.
    virtual unsigned getByValTypeAlignment(Type *Ty) const;

    /// getOptimalMemOpType - Returns the target specific optimal type for load
    /// and store operations as a result of memset, memcpy, and memmove
    /// lowering. If DstAlign is zero that means it's safe to destination
    /// alignment can satisfy any constraint. Similarly if SrcAlign is zero it
    /// means there isn't a need to check it against alignment requirement,
    /// probably because the source does not need to be loaded. If
    /// 'NonScalarIntSafe' is true, that means it's safe to return a
    /// non-scalar-integer type, e.g. empty string source, constant, or loaded
    /// from memory. 'MemcpyStrSrc' indicates whether the memcpy source is
    /// constant so it does not need to be loaded.
    /// It returns EVT::Other if the type should be determined using generic
    /// target-independent logic.
    virtual EVT
    getOptimalMemOpType(uint64_t Size, unsigned DstAlign, unsigned SrcAlign,
                        bool NonScalarIntSafe, bool MemcpyStrSrc,
                        MachineFunction &MF) const;

    /// allowsUnalignedMemoryAccesses - Returns true if the target allows
    /// unaligned memory accesses. of the specified type.
    virtual bool allowsUnalignedMemoryAccesses(EVT VT) const {
      return true;
    }

    /// LowerOperation - Provide custom lowering hooks for some operations.
    ///
    virtual SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const;

    /// ReplaceNodeResults - Replace the results of node with an illegal result
    /// type with new values built out of custom code.
    ///
    virtual void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue>&Results,
                                    SelectionDAG &DAG) const;


    virtual SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const;

    /// isTypeDesirableForOp - Return true if the target has native support for
    /// the specified value type and it is 'desirable' to use the type for the
    /// given node type. e.g. On x86 i16 is legal, but undesirable since i16
    /// instruction encodings are longer and some i16 instructions are slow.
    virtual bool isTypeDesirableForOp(unsigned Opc, EVT VT) const;

    /// isTypeDesirable - Return true if the target has native support for the
    /// specified value type and it is 'desirable' to use the type. e.g. On x86
    /// i16 is legal, but undesirable since i16 instruction encodings are longer
    /// and some i16 instructions are slow.
    virtual bool IsDesirableToPromoteOp(SDValue Op, EVT &PVT) const;

    virtual MachineBasicBlock *
      EmitInstrWithCustomInserter(MachineInstr *MI,
                                  MachineBasicBlock *MBB) const;


    /// getTargetNodeName - This method returns the name of a target specific
    /// DAG node.
    virtual const char *getTargetNodeName(unsigned Opcode) const;

    /// getSetCCResultType - Return the value type to use for ISD::SETCC.
    virtual EVT getSetCCResultType(EVT VT) const;

    /// computeMaskedBitsForTargetNode - Determine which of the bits specified
    /// in Mask are known to be either zero or one and return them in the
    /// KnownZero/KnownOne bitsets.
    virtual void computeMaskedBitsForTargetNode(const SDValue Op,
                                                const APInt &Mask,
                                                APInt &KnownZero,
                                                APInt &KnownOne,
                                                const SelectionDAG &DAG,
                                                unsigned Depth = 0) const;

    // ComputeNumSignBitsForTargetNode - Determine the number of bits in the
    // operation that are sign bits.
    virtual unsigned ComputeNumSignBitsForTargetNode(SDValue Op,
                                                     unsigned Depth) const;

    virtual bool
    isGAPlusOffset(SDNode *N, const GlobalValue* &GA, int64_t &Offset) const;

    SDValue getReturnAddressFrameIndex(SelectionDAG &DAG) const;

    virtual bool ExpandInlineAsm(CallInst *CI) const;

    ConstraintType getConstraintType(const std::string &Constraint) const;

    /// Examine constraint string and operand type and determine a weight value.
    /// The operand object must already have been set up with the operand type.
    virtual ConstraintWeight getSingleConstraintMatchWeight(
      AsmOperandInfo &info, const char *constraint) const;

    virtual const char *LowerXConstraint(EVT ConstraintVT) const;

    /// LowerAsmOperandForConstraint - Lower the specified operand into the Ops
    /// vector.  If it is invalid, don't add anything to Ops. If hasMemory is
    /// true it means one of the asm constraint of the inline asm instruction
    /// being processed is 'm'.
    virtual void LowerAsmOperandForConstraint(SDValue Op,
                                              std::string &Constraint,
                                              std::vector<SDValue> &Ops,
                                              SelectionDAG &DAG) const;

    /// getRegForInlineAsmConstraint - Given a physical register constraint
    /// (e.g. {edx}), return the register number and the register class for the
    /// register.  This should only be used for C_Register constraints.  On
    /// error, this returns a register number of 0.
    std::pair<unsigned, const TargetRegisterClass*>
      getRegForInlineAsmConstraint(const std::string &Constraint,
                                   EVT VT) const;

    /// isLegalAddressingMode - Return true if the addressing mode represented
    /// by AM is legal for this target, for a load/store of the specified type.
    virtual bool isLegalAddressingMode(const AddrMode &AM, Type *Ty)const;

    /// isTruncateFree - Return true if it's free to truncate a value of
    /// type Ty1 to type Ty2. e.g. On x86 it's free to truncate a i32 value in
    /// register EAX to i16 by referencing its sub-register AX.
    virtual bool isTruncateFree(Type *Ty1, Type *Ty2) const;
    virtual bool isTruncateFree(EVT VT1, EVT VT2) const;

    /// isZExtFree - Return true if any actual instruction that defines a
    /// value of type Ty1 implicit zero-extends the value to Ty2 in the result
    /// register. This does not necessarily include registers defined in
    /// unknown ways, such as incoming arguments, or copies from unknown
    /// virtual registers. Also, if isTruncateFree(Ty2, Ty1) is true, this
    /// does not necessarily apply to truncate instructions. e.g. on x86-64,
    /// all instructions that define 32-bit values implicit zero-extend the
    /// result out to 64 bits.
    virtual bool isZExtFree(Type *Ty1, Type *Ty2) const;
    virtual bool isZExtFree(EVT VT1, EVT VT2) const;

    /// isNarrowingProfitable - Return true if it's profitable to narrow
    /// operations of type VT1 to VT2. e.g. on x86, it's profitable to narrow
    /// from i32 to i8 but not from i32 to i16.
    virtual bool isNarrowingProfitable(EVT VT1, EVT VT2) const;

    /// isFPImmLegal - Returns true if the target can instruction select the
    /// specified FP immediate natively. If false, the legalizer will
    /// materialize the FP immediate as a load from a constant pool.
    virtual bool isFPImmLegal(const APFloat &Imm, EVT VT) const;

    /// isShuffleMaskLegal - Targets can use this to indicate that they only
    /// support *some* VECTOR_SHUFFLE operations, those with specific masks.
    /// By default, if a target supports the VECTOR_SHUFFLE node, all mask
    /// values are assumed to be legal.
    virtual bool isShuffleMaskLegal(const SmallVectorImpl<int> &Mask,
                                    EVT VT) const;

    /// isVectorClearMaskLegal - Similar to isShuffleMaskLegal. This is
    /// used by Targets can use this to indicate if there is a suitable
    /// VECTOR_SHUFFLE that can be used to replace a VAND with a constant
    /// pool entry.
    virtual bool isVectorClearMaskLegal(const SmallVectorImpl<int> &Mask,
                                        EVT VT) const;

    /// ShouldShrinkFPConstant - If true, then instruction selection should
    /// seek to shrink the FP constant of the specified type to a smaller type
    /// in order to save space and / or reduce runtime.
    virtual bool ShouldShrinkFPConstant(EVT VT) const {
      // Don't shrink FP constpool if SSE2 is available since cvtss2sd is more
      // expensive than a straight movsd. On the other hand, it's important to
      // shrink long double fp constant since fldt is very slow.
      return !X86ScalarSSEf64 || VT == MVT::f80;
    }

    const X86Subtarget* getSubtarget() const {
      return Subtarget;
    }

    /// isScalarFPTypeInSSEReg - Return true if the specified scalar FP type is
    /// computed in an SSE register, not on the X87 floating point stack.
    bool isScalarFPTypeInSSEReg(EVT VT) const {
      return (VT == MVT::f64 && X86ScalarSSEf64) || // f64 is when SSE2
      (VT == MVT::f32 && X86ScalarSSEf32);   // f32 is when SSE1
    }

    /// createFastISel - This method returns a target specific FastISel object,
    /// or null if the target does not support "fast" ISel.
    virtual FastISel *createFastISel(FunctionLoweringInfo &funcInfo) const;

    /// getStackCookieLocation - Return true if the target stores stack
    /// protector cookies at a fixed offset in some non-standard address
    /// space, and populates the address space and offset as
    /// appropriate.
    virtual bool getStackCookieLocation(unsigned &AddressSpace, unsigned &Offset) const;

    SDValue BuildFILD(SDValue Op, EVT SrcVT, SDValue Chain, SDValue StackSlot,
                      SelectionDAG &DAG) const;

  protected:
    std::pair<const TargetRegisterClass*, uint8_t>
    findRepresentativeClass(EVT VT) const;

  private:
    /// Subtarget - Keep a pointer to the X86Subtarget around so that we can
    /// make the right decision when generating code for different targets.
    const X86Subtarget *Subtarget;
    const X86RegisterInfo *RegInfo;
    const TargetData *TD;

    /// X86StackPtr - X86 physical register used as stack ptr.
    unsigned X86StackPtr;

    /// X86ScalarSSEf32, X86ScalarSSEf64 - Select between SSE or x87
    /// floating point ops.
    /// When SSE is available, use it for f32 operations.
    /// When SSE2 is available, use it for f64 operations.
    bool X86ScalarSSEf32;
    bool X86ScalarSSEf64;

    /// LegalFPImmediates - A list of legal fp immediates.
    std::vector<APFloat> LegalFPImmediates;

    /// addLegalFPImmediate - Indicate that this x86 target can instruction
    /// select the specified FP immediate natively.
    void addLegalFPImmediate(const APFloat& Imm) {
      LegalFPImmediates.push_back(Imm);
    }

    SDValue LowerCallResult(SDValue Chain, SDValue InFlag,
                            CallingConv::ID CallConv, bool isVarArg,
                            const SmallVectorImpl<ISD::InputArg> &Ins,
                            DebugLoc dl, SelectionDAG &DAG,
                            SmallVectorImpl<SDValue> &InVals) const;
    SDValue LowerMemArgument(SDValue Chain,
                             CallingConv::ID CallConv,
                             const SmallVectorImpl<ISD::InputArg> &ArgInfo,
                             DebugLoc dl, SelectionDAG &DAG,
                             const CCValAssign &VA,  MachineFrameInfo *MFI,
                              unsigned i) const;
    SDValue LowerMemOpCallTo(SDValue Chain, SDValue StackPtr, SDValue Arg,
                             DebugLoc dl, SelectionDAG &DAG,
                             const CCValAssign &VA,
                             ISD::ArgFlagsTy Flags) const;

    // Call lowering helpers.

    /// IsEligibleForTailCallOptimization - Check whether the call is eligible
    /// for tail call optimization. Targets which want to do tail call
    /// optimization should implement this function.
    bool IsEligibleForTailCallOptimization(SDValue Callee,
                                           CallingConv::ID CalleeCC,
                                           bool isVarArg,
                                           bool isCalleeStructRet,
                                           bool isCallerStructRet,
                                    const SmallVectorImpl<ISD::OutputArg> &Outs,
                                    const SmallVectorImpl<SDValue> &OutVals,
                                    const SmallVectorImpl<ISD::InputArg> &Ins,
                                           SelectionDAG& DAG) const;
    bool IsCalleePop(bool isVarArg, CallingConv::ID CallConv) const;
    SDValue EmitTailCallLoadRetAddr(SelectionDAG &DAG, SDValue &OutRetAddr,
                                SDValue Chain, bool IsTailCall, bool Is64Bit,
                                int FPDiff, DebugLoc dl) const;

    unsigned GetAlignedArgumentStackSize(unsigned StackSize,
                                         SelectionDAG &DAG) const;

    std::pair<SDValue,SDValue> FP_TO_INTHelper(SDValue Op, SelectionDAG &DAG,
                                               bool isSigned) const;

    SDValue LowerAsSplatVectorLoad(SDValue SrcOp, EVT VT, DebugLoc dl,
                                   SelectionDAG &DAG) const;
    SDValue LowerBUILD_VECTOR(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerCONCAT_VECTORS(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerVECTOR_SHUFFLE(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerEXTRACT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerEXTRACT_VECTOR_ELT_SSE4(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerINSERT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerINSERT_VECTOR_ELT_SSE4(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerSCALAR_TO_VECTOR(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerEXTRACT_SUBVECTOR(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerINSERT_SUBVECTOR(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerGlobalAddress(const GlobalValue *GV, DebugLoc dl,
                               int64_t Offset, SelectionDAG &DAG) const;
    SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerExternalSymbol(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerShiftParts(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerBITCAST(SDValue op, SelectionDAG &DAG) const;
    SDValue LowerSINT_TO_FP(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerUINT_TO_FP(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerUINT_TO_FP_i64(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerUINT_TO_FP_i32(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFP_TO_SINT(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFP_TO_UINT(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFABS(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFNEG(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFCOPYSIGN(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFGETSIGN(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerToBT(SDValue And, ISD::CondCode CC,
                      DebugLoc dl, SelectionDAG &DAG) const;
    SDValue LowerSETCC(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerVSETCC(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerSELECT(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerBRCOND(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerMEMSET(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerVACOPY(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFRAME_TO_ARGS_OFFSET(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerEH_RETURN(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerINIT_TRAMPOLINE(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerADJUST_TRAMPOLINE(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerFLT_ROUNDS_(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerCTLZ(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerCTTZ(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerADD(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerSUB(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerMUL(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerShift(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerXALUO(SDValue Op, SelectionDAG &DAG) const;

    SDValue LowerCMP_SWAP(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerLOAD_SUB(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerREADCYCLECOUNTER(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerMEMBARRIER(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerATOMIC_FENCE(SDValue Op, SelectionDAG &DAG) const;
    SDValue LowerSIGN_EXTEND_INREG(SDValue Op, SelectionDAG &DAG) const;

    // Utility functions to help LowerVECTOR_SHUFFLE
    SDValue LowerVECTOR_SHUFFLEv8i16(SDValue Op, SelectionDAG &DAG) const;

    virtual SDValue
      LowerFormalArguments(SDValue Chain,
                           CallingConv::ID CallConv, bool isVarArg,
                           const SmallVectorImpl<ISD::InputArg> &Ins,
                           DebugLoc dl, SelectionDAG &DAG,
                           SmallVectorImpl<SDValue> &InVals) const;
    virtual SDValue
      LowerCall(SDValue Chain, SDValue Callee,
                CallingConv::ID CallConv, bool isVarArg, bool &isTailCall,
                const SmallVectorImpl<ISD::OutputArg> &Outs,
                const SmallVectorImpl<SDValue> &OutVals,
                const SmallVectorImpl<ISD::InputArg> &Ins,
                DebugLoc dl, SelectionDAG &DAG,
                SmallVectorImpl<SDValue> &InVals) const;

    virtual SDValue
      LowerReturn(SDValue Chain,
                  CallingConv::ID CallConv, bool isVarArg,
                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                  const SmallVectorImpl<SDValue> &OutVals,
                  DebugLoc dl, SelectionDAG &DAG) const;

    virtual bool isUsedByReturnOnly(SDNode *N) const;

    virtual bool mayBeEmittedAsTailCall(CallInst *CI) const;

    virtual EVT
    getTypeForExtArgOrReturn(LLVMContext &Context, EVT VT,
                             ISD::NodeType ExtendKind) const;

    virtual bool
    CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
		   bool isVarArg,
		   const SmallVectorImpl<ISD::OutputArg> &Outs,
		   LLVMContext &Context) const;

    void ReplaceATOMIC_BINARY_64(SDNode *N, SmallVectorImpl<SDValue> &Results,
                                 SelectionDAG &DAG, unsigned NewOp) const;

    /// Utility function to emit string processing sse4.2 instructions
    /// that return in xmm0.
    /// This takes the instruction to expand, the associated machine basic
    /// block, the number of args, and whether or not the second arg is
    /// in memory or not.
    MachineBasicBlock *EmitPCMP(MachineInstr *BInstr, MachineBasicBlock *BB,
                                unsigned argNum, bool inMem) const;

    /// Utility functions to emit monitor and mwait instructions. These
    /// need to make sure that the arguments to the intrinsic are in the
    /// correct registers.
    MachineBasicBlock *EmitMonitor(MachineInstr *MI,
                                   MachineBasicBlock *BB) const;
    MachineBasicBlock *EmitMwait(MachineInstr *MI, MachineBasicBlock *BB) const;

    /// Utility function to emit atomic bitwise operations (and, or, xor).
    /// It takes the bitwise instruction to expand, the associated machine basic
    /// block, and the associated X86 opcodes for reg/reg and reg/imm.
    MachineBasicBlock *EmitAtomicBitwiseWithCustomInserter(
                                                    MachineInstr *BInstr,
                                                    MachineBasicBlock *BB,
                                                    unsigned regOpc,
                                                    unsigned immOpc,
                                                    unsigned loadOpc,
                                                    unsigned cxchgOpc,
                                                    unsigned notOpc,
                                                    unsigned EAXreg,
                                                    TargetRegisterClass *RC,
                                                    bool invSrc = false) const;

    MachineBasicBlock *EmitAtomicBit6432WithCustomInserter(
                                                    MachineInstr *BInstr,
                                                    MachineBasicBlock *BB,
                                                    unsigned regOpcL,
                                                    unsigned regOpcH,
                                                    unsigned immOpcL,
                                                    unsigned immOpcH,
                                                    bool invSrc = false) const;

    /// Utility function to emit atomic min and max.  It takes the min/max
    /// instruction to expand, the associated basic block, and the associated
    /// cmov opcode for moving the min or max value.
    MachineBasicBlock *EmitAtomicMinMaxWithCustomInserter(MachineInstr *BInstr,
                                                          MachineBasicBlock *BB,
                                                        unsigned cmovOpc) const;

    // Utility function to emit the low-level va_arg code for X86-64.
    MachineBasicBlock *EmitVAARG64WithCustomInserter(
                       MachineInstr *MI,
                       MachineBasicBlock *MBB) const;

    /// Utility function to emit the xmm reg save portion of va_start.
    MachineBasicBlock *EmitVAStartSaveXMMRegsWithCustomInserter(
                                                   MachineInstr *BInstr,
                                                   MachineBasicBlock *BB) const;

    MachineBasicBlock *EmitLoweredSelect(MachineInstr *I,
                                         MachineBasicBlock *BB) const;

    MachineBasicBlock *EmitLoweredWinAlloca(MachineInstr *MI,
                                              MachineBasicBlock *BB) const;

    MachineBasicBlock *EmitLoweredSegAlloca(MachineInstr *MI,
                                            MachineBasicBlock *BB,
                                            bool Is64Bit) const;

    MachineBasicBlock *EmitLoweredTLSCall(MachineInstr *MI,
                                          MachineBasicBlock *BB) const;

    MachineBasicBlock *emitLoweredTLSAddr(MachineInstr *MI,
                                          MachineBasicBlock *BB) const;

    /// Emit nodes that will be selected as "test Op0,Op0", or something
    /// equivalent, for use with the given x86 condition code.
    SDValue EmitTest(SDValue Op0, unsigned X86CC, SelectionDAG &DAG) const;

    /// Emit nodes that will be selected as "cmp Op0,Op1", or something
    /// equivalent, for use with the given x86 condition code.
    SDValue EmitCmp(SDValue Op0, SDValue Op1, unsigned X86CC,
                    SelectionDAG &DAG) const;
  };

  namespace X86 {
    FastISel *createFastISel(FunctionLoweringInfo &funcInfo);
  }
}

#endif    // X86ISELLOWERING_H
