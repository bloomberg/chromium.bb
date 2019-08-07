//===-- ARMAsmParser.cpp - Parse ARM assembly to MCInst instructions ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ARMBaseInfo.h"
#include "MCTargetDesc/ARMAddressingModes.h"
#include "MCTargetDesc/ARMMCExpr.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetAsmParser.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"

using namespace llvm;

namespace {

class ARMOperand;

class ARMAsmParser : public MCTargetAsmParser {
  MCSubtargetInfo &STI;
  MCAsmParser &Parser;

  struct {
    ARMCC::CondCodes Cond;    // Condition for IT block.
    unsigned Mask:4;          // Condition mask for instructions.
                              // Starting at first 1 (from lsb).
                              //   '1'  condition as indicated in IT.
                              //   '0'  inverse of condition (else).
                              // Count of instructions in IT block is
                              // 4 - trailingzeroes(mask)

    bool FirstCond;           // Explicit flag for when we're parsing the
                              // First instruction in the IT block. It's
                              // implied in the mask, so needs special
                              // handling.

    unsigned CurPosition;     // Current position in parsing of IT
                              // block. In range [0,3]. Initialized
                              // according to count of instructions in block.
                              // ~0U if no active IT block.
  } ITState;
  bool inITBlock() { return ITState.CurPosition != ~0U;}
  void forwardITPosition() {
    if (!inITBlock()) return;
    // Move to the next instruction in the IT block, if there is one. If not,
    // mark the block as done.
    unsigned TZ = CountTrailingZeros_32(ITState.Mask);
    if (++ITState.CurPosition == 5 - TZ)
      ITState.CurPosition = ~0U; // Done with the IT block after this.
  }


  MCAsmParser &getParser() const { return Parser; }
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }

  void Warning(SMLoc L, const Twine &Msg) { Parser.Warning(L, Msg); }
  bool Error(SMLoc L, const Twine &Msg) { return Parser.Error(L, Msg); }

  int tryParseRegister();
  bool tryParseRegisterWithWriteBack(SmallVectorImpl<MCParsedAsmOperand*> &);
  int tryParseShiftRegister(SmallVectorImpl<MCParsedAsmOperand*> &);
  bool parseRegisterList(SmallVectorImpl<MCParsedAsmOperand*> &);
  bool parseMemory(SmallVectorImpl<MCParsedAsmOperand*> &);
  bool parseOperand(SmallVectorImpl<MCParsedAsmOperand*> &, StringRef Mnemonic);
  bool parsePrefix(ARMMCExpr::VariantKind &RefKind);
  bool parseMemRegOffsetShift(ARM_AM::ShiftOpc &ShiftType,
                              unsigned &ShiftAmount);
  bool parseDirectiveWord(unsigned Size, SMLoc L);
  bool parseDirectiveThumb(SMLoc L);
  bool parseDirectiveThumbFunc(SMLoc L);
  bool parseDirectiveCode(SMLoc L);
  bool parseDirectiveSyntax(SMLoc L);

  StringRef splitMnemonic(StringRef Mnemonic, unsigned &PredicationCode,
                          bool &CarrySetting, unsigned &ProcessorIMod,
                          StringRef &ITMask);
  void getMnemonicAcceptInfo(StringRef Mnemonic, bool &CanAcceptCarrySet,
                             bool &CanAcceptPredicationCode);

  bool isThumb() const {
    // FIXME: Can tablegen auto-generate this?
    return (STI.getFeatureBits() & ARM::ModeThumb) != 0;
  }
  bool isThumbOne() const {
    return isThumb() && (STI.getFeatureBits() & ARM::FeatureThumb2) == 0;
  }
  bool isThumbTwo() const {
    return isThumb() && (STI.getFeatureBits() & ARM::FeatureThumb2);
  }
  bool hasV6Ops() const {
    return STI.getFeatureBits() & ARM::HasV6Ops;
  }
  bool hasV7Ops() const {
    return STI.getFeatureBits() & ARM::HasV7Ops;
  }
  void SwitchMode() {
    unsigned FB = ComputeAvailableFeatures(STI.ToggleFeature(ARM::ModeThumb));
    setAvailableFeatures(FB);
  }
  bool isMClass() const {
    return STI.getFeatureBits() & ARM::FeatureMClass;
  }

  /// @name Auto-generated Match Functions
  /// {

#define GET_ASSEMBLER_HEADER
#include "ARMGenAsmMatcher.inc"

  /// }

  OperandMatchResultTy parseITCondCode(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseCoprocNumOperand(
    SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseCoprocRegOperand(
    SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseCoprocOptionOperand(
    SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseMemBarrierOptOperand(
    SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseProcIFlagsOperand(
    SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseMSRMaskOperand(
    SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parsePKHImm(SmallVectorImpl<MCParsedAsmOperand*> &O,
                                   StringRef Op, int Low, int High);
  OperandMatchResultTy parsePKHLSLImm(SmallVectorImpl<MCParsedAsmOperand*> &O) {
    return parsePKHImm(O, "lsl", 0, 31);
  }
  OperandMatchResultTy parsePKHASRImm(SmallVectorImpl<MCParsedAsmOperand*> &O) {
    return parsePKHImm(O, "asr", 1, 32);
  }
  OperandMatchResultTy parseSetEndImm(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseShifterImm(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseRotImm(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseBitfield(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parsePostIdxReg(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseAM3Offset(SmallVectorImpl<MCParsedAsmOperand*>&);
  OperandMatchResultTy parseFPImm(SmallVectorImpl<MCParsedAsmOperand*>&);

  // Asm Match Converter Methods
  bool cvtT2LdrdPre(MCInst &Inst, unsigned Opcode,
                    const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtT2StrdPre(MCInst &Inst, unsigned Opcode,
                    const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdWriteBackRegT2AddrModeImm8(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStWriteBackRegT2AddrModeImm8(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdWriteBackRegAddrMode2(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdWriteBackRegAddrModeImm12(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStWriteBackRegAddrModeImm12(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStWriteBackRegAddrMode2(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStWriteBackRegAddrMode3(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdExtTWriteBackImm(MCInst &Inst, unsigned Opcode,
                             const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdExtTWriteBackReg(MCInst &Inst, unsigned Opcode,
                             const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStExtTWriteBackImm(MCInst &Inst, unsigned Opcode,
                             const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStExtTWriteBackReg(MCInst &Inst, unsigned Opcode,
                             const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdrdPre(MCInst &Inst, unsigned Opcode,
                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtStrdPre(MCInst &Inst, unsigned Opcode,
                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtLdWriteBackRegAddrMode3(MCInst &Inst, unsigned Opcode,
                                  const SmallVectorImpl<MCParsedAsmOperand*> &);
  bool cvtThumbMultiply(MCInst &Inst, unsigned Opcode,
                        const SmallVectorImpl<MCParsedAsmOperand*> &);

  bool validateInstruction(MCInst &Inst,
                           const SmallVectorImpl<MCParsedAsmOperand*> &Ops);
  void processInstruction(MCInst &Inst,
                          const SmallVectorImpl<MCParsedAsmOperand*> &Ops);
  bool shouldOmitCCOutOperand(StringRef Mnemonic,
                              SmallVectorImpl<MCParsedAsmOperand*> &Operands);

public:
  enum ARMMatchResultTy {
    Match_RequiresITBlock = FIRST_TARGET_MATCH_RESULT_TY,
    Match_RequiresNotITBlock,
    Match_RequiresV6,
    Match_RequiresThumb2
  };

  ARMAsmParser(MCSubtargetInfo &_STI, MCAsmParser &_Parser)
    : MCTargetAsmParser(), STI(_STI), Parser(_Parser) {
    MCAsmParserExtension::Initialize(_Parser);

    // Initialize the set of available features.
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));

    // Not in an ITBlock to start with.
    ITState.CurPosition = ~0U;
  }

  // Implementation of the MCTargetAsmParser interface:
  bool ParseRegister(unsigned &RegNo, SMLoc &StartLoc, SMLoc &EndLoc);
  bool ParseInstruction(StringRef Name, SMLoc NameLoc,
                        SmallVectorImpl<MCParsedAsmOperand*> &Operands);
  bool ParseDirective(AsmToken DirectiveID);

  unsigned checkTargetMatchPredicate(MCInst &Inst);

  bool MatchAndEmitInstruction(SMLoc IDLoc,
                               SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                               MCStreamer &Out);
};
} // end anonymous namespace

namespace {

/// ARMOperand - Instances of this class represent a parsed ARM machine
/// instruction.
class ARMOperand : public MCParsedAsmOperand {
  enum KindTy {
    k_CondCode,
    k_CCOut,
    k_ITCondMask,
    k_CoprocNum,
    k_CoprocReg,
    k_CoprocOption,
    k_Immediate,
    k_FPImmediate,
    k_MemBarrierOpt,
    k_Memory,
    k_PostIndexRegister,
    k_MSRMask,
    k_ProcIFlags,
    k_VectorIndex,
    k_Register,
    k_RegisterList,
    k_DPRRegisterList,
    k_SPRRegisterList,
    k_ShiftedRegister,
    k_ShiftedImmediate,
    k_ShifterImmediate,
    k_RotateImmediate,
    k_BitfieldDescriptor,
    k_Token
  } Kind;

  SMLoc StartLoc, EndLoc;
  SmallVector<unsigned, 8> Registers;

  union {
    struct {
      ARMCC::CondCodes Val;
    } CC;

    struct {
      unsigned Val;
    } Cop;

    struct {
      unsigned Val;
    } CoprocOption;

    struct {
      unsigned Mask:4;
    } ITMask;

    struct {
      ARM_MB::MemBOpt Val;
    } MBOpt;

    struct {
      ARM_PROC::IFlags Val;
    } IFlags;

    struct {
      unsigned Val;
    } MMask;

    struct {
      const char *Data;
      unsigned Length;
    } Tok;

    struct {
      unsigned RegNum;
    } Reg;

    struct {
      unsigned Val;
    } VectorIndex;

    struct {
      const MCExpr *Val;
    } Imm;

    struct {
      unsigned Val;       // encoded 8-bit representation
    } FPImm;

    /// Combined record for all forms of ARM address expressions.
    struct {
      unsigned BaseRegNum;
      // Offset is in OffsetReg or OffsetImm. If both are zero, no offset
      // was specified.
      const MCConstantExpr *OffsetImm;  // Offset immediate value
      unsigned OffsetRegNum;    // Offset register num, when OffsetImm == NULL
      ARM_AM::ShiftOpc ShiftType; // Shift type for OffsetReg
      unsigned ShiftImm;        // shift for OffsetReg.
      unsigned Alignment;       // 0 = no alignment specified
                                // n = alignment in bytes (8, 16, or 32)
      unsigned isNegative : 1;  // Negated OffsetReg? (~'U' bit)
    } Memory;

    struct {
      unsigned RegNum;
      bool isAdd;
      ARM_AM::ShiftOpc ShiftTy;
      unsigned ShiftImm;
    } PostIdxReg;

    struct {
      bool isASR;
      unsigned Imm;
    } ShifterImm;
    struct {
      ARM_AM::ShiftOpc ShiftTy;
      unsigned SrcReg;
      unsigned ShiftReg;
      unsigned ShiftImm;
    } RegShiftedReg;
    struct {
      ARM_AM::ShiftOpc ShiftTy;
      unsigned SrcReg;
      unsigned ShiftImm;
    } RegShiftedImm;
    struct {
      unsigned Imm;
    } RotImm;
    struct {
      unsigned LSB;
      unsigned Width;
    } Bitfield;
  };

  ARMOperand(KindTy K) : MCParsedAsmOperand(), Kind(K) {}
public:
  ARMOperand(const ARMOperand &o) : MCParsedAsmOperand() {
    Kind = o.Kind;
    StartLoc = o.StartLoc;
    EndLoc = o.EndLoc;
    switch (Kind) {
    case k_CondCode:
      CC = o.CC;
      break;
    case k_ITCondMask:
      ITMask = o.ITMask;
      break;
    case k_Token:
      Tok = o.Tok;
      break;
    case k_CCOut:
    case k_Register:
      Reg = o.Reg;
      break;
    case k_RegisterList:
    case k_DPRRegisterList:
    case k_SPRRegisterList:
      Registers = o.Registers;
      break;
    case k_CoprocNum:
    case k_CoprocReg:
      Cop = o.Cop;
      break;
    case k_CoprocOption:
      CoprocOption = o.CoprocOption;
      break;
    case k_Immediate:
      Imm = o.Imm;
      break;
    case k_FPImmediate:
      FPImm = o.FPImm;
      break;
    case k_MemBarrierOpt:
      MBOpt = o.MBOpt;
      break;
    case k_Memory:
      Memory = o.Memory;
      break;
    case k_PostIndexRegister:
      PostIdxReg = o.PostIdxReg;
      break;
    case k_MSRMask:
      MMask = o.MMask;
      break;
    case k_ProcIFlags:
      IFlags = o.IFlags;
      break;
    case k_ShifterImmediate:
      ShifterImm = o.ShifterImm;
      break;
    case k_ShiftedRegister:
      RegShiftedReg = o.RegShiftedReg;
      break;
    case k_ShiftedImmediate:
      RegShiftedImm = o.RegShiftedImm;
      break;
    case k_RotateImmediate:
      RotImm = o.RotImm;
      break;
    case k_BitfieldDescriptor:
      Bitfield = o.Bitfield;
      break;
    case k_VectorIndex:
      VectorIndex = o.VectorIndex;
      break;
    }
  }

  /// getStartLoc - Get the location of the first token of this operand.
  SMLoc getStartLoc() const { return StartLoc; }
  /// getEndLoc - Get the location of the last token of this operand.
  SMLoc getEndLoc() const { return EndLoc; }

  ARMCC::CondCodes getCondCode() const {
    assert(Kind == k_CondCode && "Invalid access!");
    return CC.Val;
  }

  unsigned getCoproc() const {
    assert((Kind == k_CoprocNum || Kind == k_CoprocReg) && "Invalid access!");
    return Cop.Val;
  }

  StringRef getToken() const {
    assert(Kind == k_Token && "Invalid access!");
    return StringRef(Tok.Data, Tok.Length);
  }

  unsigned getReg() const {
    assert((Kind == k_Register || Kind == k_CCOut) && "Invalid access!");
    return Reg.RegNum;
  }

  const SmallVectorImpl<unsigned> &getRegList() const {
    assert((Kind == k_RegisterList || Kind == k_DPRRegisterList ||
            Kind == k_SPRRegisterList) && "Invalid access!");
    return Registers;
  }

  const MCExpr *getImm() const {
    assert(Kind == k_Immediate && "Invalid access!");
    return Imm.Val;
  }

  unsigned getFPImm() const {
    assert(Kind == k_FPImmediate && "Invalid access!");
    return FPImm.Val;
  }

  unsigned getVectorIndex() const {
    assert(Kind == k_VectorIndex && "Invalid access!");
    return VectorIndex.Val;
  }

  ARM_MB::MemBOpt getMemBarrierOpt() const {
    assert(Kind == k_MemBarrierOpt && "Invalid access!");
    return MBOpt.Val;
  }

  ARM_PROC::IFlags getProcIFlags() const {
    assert(Kind == k_ProcIFlags && "Invalid access!");
    return IFlags.Val;
  }

  unsigned getMSRMask() const {
    assert(Kind == k_MSRMask && "Invalid access!");
    return MMask.Val;
  }

  bool isCoprocNum() const { return Kind == k_CoprocNum; }
  bool isCoprocReg() const { return Kind == k_CoprocReg; }
  bool isCoprocOption() const { return Kind == k_CoprocOption; }
  bool isCondCode() const { return Kind == k_CondCode; }
  bool isCCOut() const { return Kind == k_CCOut; }
  bool isITMask() const { return Kind == k_ITCondMask; }
  bool isITCondCode() const { return Kind == k_CondCode; }
  bool isImm() const { return Kind == k_Immediate; }
  bool isFPImm() const { return Kind == k_FPImmediate; }
  bool isImm8s4() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return ((Value & 3) == 0) && Value >= -1020 && Value <= 1020;
  }
  bool isImm0_1020s4() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return ((Value & 3) == 0) && Value >= 0 && Value <= 1020;
  }
  bool isImm0_508s4() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return ((Value & 3) == 0) && Value >= 0 && Value <= 508;
  }
  bool isImm0_255() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 256;
  }
  bool isImm0_7() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 8;
  }
  bool isImm0_15() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 16;
  }
  bool isImm0_31() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 32;
  }
  bool isImm1_16() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value > 0 && Value < 17;
  }
  bool isImm1_32() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value > 0 && Value < 33;
  }
  bool isImm0_65535() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 65536;
  }
  bool isImm0_65535Expr() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    // If it's not a constant expression, it'll generate a fixup and be
    // handled later.
    if (!CE) return true;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 65536;
  }
  bool isImm24bit() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value <= 0xffffff;
  }
  bool isImmThumbSR() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value > 0 && Value < 33;
  }
  bool isPKHLSLImm() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value >= 0 && Value < 32;
  }
  bool isPKHASRImm() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value > 0 && Value <= 32;
  }
  bool isARMSOImm() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return ARM_AM::getSOImmVal(Value) != -1;
  }
  bool isT2SOImm() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return ARM_AM::getT2SOImmVal(Value) != -1;
  }
  bool isSetEndImm() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Value = CE->getValue();
    return Value == 1 || Value == 0;
  }
  bool isReg() const { return Kind == k_Register; }
  bool isRegList() const { return Kind == k_RegisterList; }
  bool isDPRRegList() const { return Kind == k_DPRRegisterList; }
  bool isSPRRegList() const { return Kind == k_SPRRegisterList; }
  bool isToken() const { return Kind == k_Token; }
  bool isMemBarrierOpt() const { return Kind == k_MemBarrierOpt; }
  bool isMemory() const { return Kind == k_Memory; }
  bool isShifterImm() const { return Kind == k_ShifterImmediate; }
  bool isRegShiftedReg() const { return Kind == k_ShiftedRegister; }
  bool isRegShiftedImm() const { return Kind == k_ShiftedImmediate; }
  bool isRotImm() const { return Kind == k_RotateImmediate; }
  bool isBitfield() const { return Kind == k_BitfieldDescriptor; }
  bool isPostIdxRegShifted() const { return Kind == k_PostIndexRegister; }
  bool isPostIdxReg() const {
    return Kind == k_PostIndexRegister && PostIdxReg.ShiftTy == ARM_AM::no_shift;
  }
  bool isMemNoOffset(bool alignOK = false) const {
    if (!isMemory())
      return false;
    // No offset of any kind.
    return Memory.OffsetRegNum == 0 && Memory.OffsetImm == 0 &&
     (alignOK || Memory.Alignment == 0);
  }
  bool isAlignedMemory() const {
    return isMemNoOffset(true);
  }
  bool isAddrMode2() const {
    if (!isMemory() || Memory.Alignment != 0) return false;
    // Check for register offset.
    if (Memory.OffsetRegNum) return true;
    // Immediate offset in range [-4095, 4095].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val > -4096 && Val < 4096;
  }
  bool isAM2OffsetImm() const {
    if (Kind != k_Immediate)
      return false;
    // Immediate offset in range [-4095, 4095].
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Val = CE->getValue();
    return Val > -4096 && Val < 4096;
  }
  bool isAddrMode3() const {
    if (!isMemory() || Memory.Alignment != 0) return false;
    // No shifts are legal for AM3.
    if (Memory.ShiftType != ARM_AM::no_shift) return false;
    // Check for register offset.
    if (Memory.OffsetRegNum) return true;
    // Immediate offset in range [-255, 255].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val > -256 && Val < 256;
  }
  bool isAM3Offset() const {
    if (Kind != k_Immediate && Kind != k_PostIndexRegister)
      return false;
    if (Kind == k_PostIndexRegister)
      return PostIdxReg.ShiftTy == ARM_AM::no_shift;
    // Immediate offset in range [-255, 255].
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Val = CE->getValue();
    // Special case, #-0 is INT32_MIN.
    return (Val > -256 && Val < 256) || Val == INT32_MIN;
  }
  bool isAddrMode5() const {
    if (!isMemory() || Memory.Alignment != 0) return false;
    // Check for register offset.
    if (Memory.OffsetRegNum) return false;
    // Immediate offset in range [-1020, 1020] and a multiple of 4.
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return (Val >= -1020 && Val <= 1020 && ((Val & 3) == 0)) ||
           Val == INT32_MIN;
  }
  bool isMemTBB() const {
    if (!isMemory() || !Memory.OffsetRegNum || Memory.isNegative ||
        Memory.ShiftType != ARM_AM::no_shift || Memory.Alignment != 0)
      return false;
    return true;
  }
  bool isMemTBH() const {
    if (!isMemory() || !Memory.OffsetRegNum || Memory.isNegative ||
        Memory.ShiftType != ARM_AM::lsl || Memory.ShiftImm != 1 ||
        Memory.Alignment != 0 )
      return false;
    return true;
  }
  bool isMemRegOffset() const {
    if (!isMemory() || !Memory.OffsetRegNum || Memory.Alignment != 0)
      return false;
    return true;
  }
  bool isT2MemRegOffset() const {
    if (!isMemory() || !Memory.OffsetRegNum || Memory.isNegative ||
        Memory.Alignment != 0)
      return false;
    // Only lsl #{0, 1, 2, 3} allowed.
    if (Memory.ShiftType == ARM_AM::no_shift)
      return true;
    if (Memory.ShiftType != ARM_AM::lsl || Memory.ShiftImm > 3)
      return false;
    return true;
  }
  bool isMemThumbRR() const {
    // Thumb reg+reg addressing is simple. Just two registers, a base and
    // an offset. No shifts, negations or any other complicating factors.
    if (!isMemory() || !Memory.OffsetRegNum || Memory.isNegative ||
        Memory.ShiftType != ARM_AM::no_shift || Memory.Alignment != 0)
      return false;
    return isARMLowRegister(Memory.BaseRegNum) &&
      (!Memory.OffsetRegNum || isARMLowRegister(Memory.OffsetRegNum));
  }
  bool isMemThumbRIs4() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 ||
        !isARMLowRegister(Memory.BaseRegNum) || Memory.Alignment != 0)
      return false;
    // Immediate offset, multiple of 4 in range [0, 124].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= 0 && Val <= 124 && (Val % 4) == 0;
  }
  bool isMemThumbRIs2() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 ||
        !isARMLowRegister(Memory.BaseRegNum) || Memory.Alignment != 0)
      return false;
    // Immediate offset, multiple of 4 in range [0, 62].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= 0 && Val <= 62 && (Val % 2) == 0;
  }
  bool isMemThumbRIs1() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 ||
        !isARMLowRegister(Memory.BaseRegNum) || Memory.Alignment != 0)
      return false;
    // Immediate offset in range [0, 31].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= 0 && Val <= 31;
  }
  bool isMemThumbSPI() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 ||
        Memory.BaseRegNum != ARM::SP || Memory.Alignment != 0)
      return false;
    // Immediate offset, multiple of 4 in range [0, 1020].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= 0 && Val <= 1020 && (Val % 4) == 0;
  }
  bool isMemImm8s4Offset() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset a multiple of 4 in range [-1020, 1020].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= -1020 && Val <= 1020 && (Val & 3) == 0;
  }
  bool isMemImm0_1020s4Offset() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset a multiple of 4 in range [0, 1020].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= 0 && Val <= 1020 && (Val & 3) == 0;
  }
  bool isMemImm8Offset() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset in range [-255, 255].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return (Val == INT32_MIN) || (Val > -256 && Val < 256);
  }
  bool isMemPosImm8Offset() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset in range [0, 255].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val >= 0 && Val < 256;
  }
  bool isMemNegImm8Offset() const {
    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset in range [-255, -1].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return Val > -256 && Val < 0;
  }
  bool isMemUImm12Offset() const {
    // If we have an immediate that's not a constant, treat it as a label
    // reference needing a fixup. If it is a constant, it's something else
    // and we reject it.
    if (Kind == k_Immediate && !isa<MCConstantExpr>(getImm()))
      return true;

    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset in range [0, 4095].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return (Val >= 0 && Val < 4096);
  }
  bool isMemImm12Offset() const {
    // If we have an immediate that's not a constant, treat it as a label
    // reference needing a fixup. If it is a constant, it's something else
    // and we reject it.
    if (Kind == k_Immediate && !isa<MCConstantExpr>(getImm()))
      return true;

    if (!isMemory() || Memory.OffsetRegNum != 0 || Memory.Alignment != 0)
      return false;
    // Immediate offset in range [-4095, 4095].
    if (!Memory.OffsetImm) return true;
    int64_t Val = Memory.OffsetImm->getValue();
    return (Val > -4096 && Val < 4096) || (Val == INT32_MIN);
  }
  bool isPostIdxImm8() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Val = CE->getValue();
    return (Val > -256 && Val < 256) || (Val == INT32_MIN);
  }
  bool isPostIdxImm8s4() const {
    if (Kind != k_Immediate)
      return false;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) return false;
    int64_t Val = CE->getValue();
    return ((Val & 3) == 0 && Val >= -1020 && Val <= 1020) ||
      (Val == INT32_MIN);
  }

  bool isMSRMask() const { return Kind == k_MSRMask; }
  bool isProcIFlags() const { return Kind == k_ProcIFlags; }

  bool isVectorIndex8() const {
    if (Kind != k_VectorIndex) return false;
    return VectorIndex.Val < 8;
  }
  bool isVectorIndex16() const {
    if (Kind != k_VectorIndex) return false;
    return VectorIndex.Val < 4;
  }
  bool isVectorIndex32() const {
    if (Kind != k_VectorIndex) return false;
    return VectorIndex.Val < 2;
  }



  void addExpr(MCInst &Inst, const MCExpr *Expr) const {
    // Add as immediates when possible.  Null MCExpr = 0.
    if (Expr == 0)
      Inst.addOperand(MCOperand::CreateImm(0));
    else if (const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::CreateImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::CreateExpr(Expr));
  }

  void addCondCodeOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(unsigned(getCondCode())));
    unsigned RegNum = getCondCode() == ARMCC::AL ? 0: ARM::CPSR;
    Inst.addOperand(MCOperand::CreateReg(RegNum));
  }

  void addCoprocNumOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(getCoproc()));
  }

  void addCoprocRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(getCoproc()));
  }

  void addCoprocOptionOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(CoprocOption.Val));
  }

  void addITMaskOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(ITMask.Mask));
  }

  void addITCondCodeOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(unsigned(getCondCode())));
  }

  void addCCOutOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(getReg()));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(getReg()));
  }

  void addRegShiftedRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 3 && "Invalid number of operands!");
    assert(isRegShiftedReg() && "addRegShiftedRegOperands() on non RegShiftedReg!");
    Inst.addOperand(MCOperand::CreateReg(RegShiftedReg.SrcReg));
    Inst.addOperand(MCOperand::CreateReg(RegShiftedReg.ShiftReg));
    Inst.addOperand(MCOperand::CreateImm(
      ARM_AM::getSORegOpc(RegShiftedReg.ShiftTy, RegShiftedReg.ShiftImm)));
  }

  void addRegShiftedImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    assert(isRegShiftedImm() && "addRegShiftedImmOperands() on non RegShiftedImm!");
    Inst.addOperand(MCOperand::CreateReg(RegShiftedImm.SrcReg));
    Inst.addOperand(MCOperand::CreateImm(
      ARM_AM::getSORegOpc(RegShiftedImm.ShiftTy, RegShiftedImm.ShiftImm)));
  }

  void addShifterImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm((ShifterImm.isASR << 5) |
                                         ShifterImm.Imm));
  }

  void addRegListOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    const SmallVectorImpl<unsigned> &RegList = getRegList();
    for (SmallVectorImpl<unsigned>::const_iterator
           I = RegList.begin(), E = RegList.end(); I != E; ++I)
      Inst.addOperand(MCOperand::CreateReg(*I));
  }

  void addDPRRegListOperands(MCInst &Inst, unsigned N) const {
    addRegListOperands(Inst, N);
  }

  void addSPRRegListOperands(MCInst &Inst, unsigned N) const {
    addRegListOperands(Inst, N);
  }

  void addRotImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // Encoded as val>>3. The printer handles display as 8, 16, 24.
    Inst.addOperand(MCOperand::CreateImm(RotImm.Imm >> 3));
  }

  void addBitfieldOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // Munge the lsb/width into a bitfield mask.
    unsigned lsb = Bitfield.LSB;
    unsigned width = Bitfield.Width;
    // Make a 32-bit mask w/ the referenced bits clear and all other bits set.
    uint32_t Mask = ~(((uint32_t)0xffffffff >> lsb) << (32 - width) >>
                      (32 - (lsb + width)));
    Inst.addOperand(MCOperand::CreateImm(Mask));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addFPImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(getFPImm()));
  }

  void addImm8s4Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // FIXME: We really want to scale the value here, but the LDRD/STRD
    // instruction don't encode operands that way yet.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    Inst.addOperand(MCOperand::CreateImm(CE->getValue()));
  }

  void addImm0_1020s4Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // The immediate is scaled by four in the encoding and is stored
    // in the MCInst as such. Lop off the low two bits here.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    Inst.addOperand(MCOperand::CreateImm(CE->getValue() / 4));
  }

  void addImm0_508s4Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // The immediate is scaled by four in the encoding and is stored
    // in the MCInst as such. Lop off the low two bits here.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    Inst.addOperand(MCOperand::CreateImm(CE->getValue() / 4));
  }

  void addImm0_255Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm0_7Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm0_15Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm0_31Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm1_16Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // The constant encodes as the immediate-1, and we store in the instruction
    // the bits as encoded, so subtract off one here.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    Inst.addOperand(MCOperand::CreateImm(CE->getValue() - 1));
  }

  void addImm1_32Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // The constant encodes as the immediate-1, and we store in the instruction
    // the bits as encoded, so subtract off one here.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    Inst.addOperand(MCOperand::CreateImm(CE->getValue() - 1));
  }

  void addImm0_65535Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm0_65535ExprOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImm24bitOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addImmThumbSROperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // The constant encodes as the immediate, except for 32, which encodes as
    // zero.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    unsigned Imm = CE->getValue();
    Inst.addOperand(MCOperand::CreateImm((Imm == 32 ? 0 : Imm)));
  }

  void addPKHLSLImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addPKHASRImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    // An ASR value of 32 encodes as 0, so that's how we want to add it to
    // the instruction as well.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    int Val = CE->getValue();
    Inst.addOperand(MCOperand::CreateImm(Val == 32 ? 0 : Val));
  }

  void addARMSOImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addT2SOImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addSetEndImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  void addMemBarrierOptOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(unsigned(getMemBarrierOpt())));
  }

  void addMemNoOffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
  }

  void addAlignedMemoryOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Memory.Alignment));
  }

  void addAddrMode2Operands(MCInst &Inst, unsigned N) const {
    assert(N == 3 && "Invalid number of operands!");
    int32_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() : 0;
    if (!Memory.OffsetRegNum) {
      ARM_AM::AddrOpc AddSub = Val < 0 ? ARM_AM::sub : ARM_AM::add;
      // Special case for #-0
      if (Val == INT32_MIN) Val = 0;
      if (Val < 0) Val = -Val;
      Val = ARM_AM::getAM2Opc(AddSub, Val, ARM_AM::no_shift);
    } else {
      // For register offset, we encode the shift type and negation flag
      // here.
      Val = ARM_AM::getAM2Opc(Memory.isNegative ? ARM_AM::sub : ARM_AM::add,
                              Memory.ShiftImm, Memory.ShiftType);
    }
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addAM2OffsetImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    assert(CE && "non-constant AM2OffsetImm operand!");
    int32_t Val = CE->getValue();
    ARM_AM::AddrOpc AddSub = Val < 0 ? ARM_AM::sub : ARM_AM::add;
    // Special case for #-0
    if (Val == INT32_MIN) Val = 0;
    if (Val < 0) Val = -Val;
    Val = ARM_AM::getAM2Opc(AddSub, Val, ARM_AM::no_shift);
    Inst.addOperand(MCOperand::CreateReg(0));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addAddrMode3Operands(MCInst &Inst, unsigned N) const {
    assert(N == 3 && "Invalid number of operands!");
    int32_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() : 0;
    if (!Memory.OffsetRegNum) {
      ARM_AM::AddrOpc AddSub = Val < 0 ? ARM_AM::sub : ARM_AM::add;
      // Special case for #-0
      if (Val == INT32_MIN) Val = 0;
      if (Val < 0) Val = -Val;
      Val = ARM_AM::getAM3Opc(AddSub, Val);
    } else {
      // For register offset, we encode the shift type and negation flag
      // here.
      Val = ARM_AM::getAM3Opc(Memory.isNegative ? ARM_AM::sub : ARM_AM::add, 0);
    }
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addAM3OffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    if (Kind == k_PostIndexRegister) {
      int32_t Val =
        ARM_AM::getAM3Opc(PostIdxReg.isAdd ? ARM_AM::add : ARM_AM::sub, 0);
      Inst.addOperand(MCOperand::CreateReg(PostIdxReg.RegNum));
      Inst.addOperand(MCOperand::CreateImm(Val));
      return;
    }

    // Constant offset.
    const MCConstantExpr *CE = static_cast<const MCConstantExpr*>(getImm());
    int32_t Val = CE->getValue();
    ARM_AM::AddrOpc AddSub = Val < 0 ? ARM_AM::sub : ARM_AM::add;
    // Special case for #-0
    if (Val == INT32_MIN) Val = 0;
    if (Val < 0) Val = -Val;
    Val = ARM_AM::getAM3Opc(AddSub, Val);
    Inst.addOperand(MCOperand::CreateReg(0));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addAddrMode5Operands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    // The lower two bits are always zero and as such are not encoded.
    int32_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() / 4 : 0;
    ARM_AM::AddrOpc AddSub = Val < 0 ? ARM_AM::sub : ARM_AM::add;
    // Special case for #-0
    if (Val == INT32_MIN) Val = 0;
    if (Val < 0) Val = -Val;
    Val = ARM_AM::getAM5Opc(AddSub, Val);
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemImm8s4OffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    int64_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemImm0_1020s4OffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    // The lower two bits are always zero and as such are not encoded.
    int32_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() / 4 : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemImm8OffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    int64_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemPosImm8OffsetOperands(MCInst &Inst, unsigned N) const {
    addMemImm8OffsetOperands(Inst, N);
  }

  void addMemNegImm8OffsetOperands(MCInst &Inst, unsigned N) const {
    addMemImm8OffsetOperands(Inst, N);
  }

  void addMemUImm12OffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    // If this is an immediate, it's a label reference.
    if (Kind == k_Immediate) {
      addExpr(Inst, getImm());
      Inst.addOperand(MCOperand::CreateImm(0));
      return;
    }

    // Otherwise, it's a normal memory reg+offset.
    int64_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemImm12OffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    // If this is an immediate, it's a label reference.
    if (Kind == k_Immediate) {
      addExpr(Inst, getImm());
      Inst.addOperand(MCOperand::CreateImm(0));
      return;
    }

    // Otherwise, it's a normal memory reg+offset.
    int64_t Val = Memory.OffsetImm ? Memory.OffsetImm->getValue() : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemTBBOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
  }

  void addMemTBHOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
  }

  void addMemRegOffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 3 && "Invalid number of operands!");
    unsigned Val = ARM_AM::getAM2Opc(Memory.isNegative ? ARM_AM::sub : ARM_AM::add,
                                     Memory.ShiftImm, Memory.ShiftType);
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addT2MemRegOffsetOperands(MCInst &Inst, unsigned N) const {
    assert(N == 3 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
    Inst.addOperand(MCOperand::CreateImm(Memory.ShiftImm));
  }

  void addMemThumbRROperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateReg(Memory.OffsetRegNum));
  }

  void addMemThumbRIs4Operands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    int64_t Val = Memory.OffsetImm ? (Memory.OffsetImm->getValue() / 4) : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemThumbRIs2Operands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    int64_t Val = Memory.OffsetImm ? (Memory.OffsetImm->getValue() / 2) : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemThumbRIs1Operands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    int64_t Val = Memory.OffsetImm ? (Memory.OffsetImm->getValue()) : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addMemThumbSPIOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    int64_t Val = Memory.OffsetImm ? (Memory.OffsetImm->getValue() / 4) : 0;
    Inst.addOperand(MCOperand::CreateReg(Memory.BaseRegNum));
    Inst.addOperand(MCOperand::CreateImm(Val));
  }

  void addPostIdxImm8Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    assert(CE && "non-constant post-idx-imm8 operand!");
    int Imm = CE->getValue();
    bool isAdd = Imm >= 0;
    if (Imm == INT32_MIN) Imm = 0;
    Imm = (Imm < 0 ? -Imm : Imm) | (int)isAdd << 8;
    Inst.addOperand(MCOperand::CreateImm(Imm));
  }

  void addPostIdxImm8s4Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(getImm());
    assert(CE && "non-constant post-idx-imm8s4 operand!");
    int Imm = CE->getValue();
    bool isAdd = Imm >= 0;
    if (Imm == INT32_MIN) Imm = 0;
    // Immediate is scaled by 4.
    Imm = ((Imm < 0 ? -Imm : Imm) / 4) | (int)isAdd << 8;
    Inst.addOperand(MCOperand::CreateImm(Imm));
  }

  void addPostIdxRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(PostIdxReg.RegNum));
    Inst.addOperand(MCOperand::CreateImm(PostIdxReg.isAdd));
  }

  void addPostIdxRegShiftedOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(PostIdxReg.RegNum));
    // The sign, shift type, and shift amount are encoded in a single operand
    // using the AM2 encoding helpers.
    ARM_AM::AddrOpc opc = PostIdxReg.isAdd ? ARM_AM::add : ARM_AM::sub;
    unsigned Imm = ARM_AM::getAM2Opc(opc, PostIdxReg.ShiftImm,
                                     PostIdxReg.ShiftTy);
    Inst.addOperand(MCOperand::CreateImm(Imm));
  }

  void addMSRMaskOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(unsigned(getMSRMask())));
  }

  void addProcIFlagsOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(unsigned(getProcIFlags())));
  }

  void addVectorIndex8Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(getVectorIndex()));
  }

  void addVectorIndex16Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(getVectorIndex()));
  }

  void addVectorIndex32Operands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateImm(getVectorIndex()));
  }

  virtual void print(raw_ostream &OS) const;

  static ARMOperand *CreateITMask(unsigned Mask, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_ITCondMask);
    Op->ITMask.Mask = Mask;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateCondCode(ARMCC::CondCodes CC, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_CondCode);
    Op->CC.Val = CC;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateCoprocNum(unsigned CopVal, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_CoprocNum);
    Op->Cop.Val = CopVal;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateCoprocReg(unsigned CopVal, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_CoprocReg);
    Op->Cop.Val = CopVal;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateCoprocOption(unsigned Val, SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_CoprocOption);
    Op->Cop.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateCCOut(unsigned RegNum, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_CCOut);
    Op->Reg.RegNum = RegNum;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateToken(StringRef Str, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_Token);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateReg(unsigned RegNum, SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_Register);
    Op->Reg.RegNum = RegNum;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateShiftedRegister(ARM_AM::ShiftOpc ShTy,
                                           unsigned SrcReg,
                                           unsigned ShiftReg,
                                           unsigned ShiftImm,
                                           SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_ShiftedRegister);
    Op->RegShiftedReg.ShiftTy = ShTy;
    Op->RegShiftedReg.SrcReg = SrcReg;
    Op->RegShiftedReg.ShiftReg = ShiftReg;
    Op->RegShiftedReg.ShiftImm = ShiftImm;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateShiftedImmediate(ARM_AM::ShiftOpc ShTy,
                                            unsigned SrcReg,
                                            unsigned ShiftImm,
                                            SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_ShiftedImmediate);
    Op->RegShiftedImm.ShiftTy = ShTy;
    Op->RegShiftedImm.SrcReg = SrcReg;
    Op->RegShiftedImm.ShiftImm = ShiftImm;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateShifterImm(bool isASR, unsigned Imm,
                                   SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_ShifterImmediate);
    Op->ShifterImm.isASR = isASR;
    Op->ShifterImm.Imm = Imm;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateRotImm(unsigned Imm, SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_RotateImmediate);
    Op->RotImm.Imm = Imm;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateBitfield(unsigned LSB, unsigned Width,
                                    SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_BitfieldDescriptor);
    Op->Bitfield.LSB = LSB;
    Op->Bitfield.Width = Width;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *
  CreateRegList(const SmallVectorImpl<std::pair<unsigned, SMLoc> > &Regs,
                SMLoc StartLoc, SMLoc EndLoc) {
    KindTy Kind = k_RegisterList;

    if (ARMMCRegisterClasses[ARM::DPRRegClassID].contains(Regs.front().first))
      Kind = k_DPRRegisterList;
    else if (ARMMCRegisterClasses[ARM::SPRRegClassID].
             contains(Regs.front().first))
      Kind = k_SPRRegisterList;

    ARMOperand *Op = new ARMOperand(Kind);
    for (SmallVectorImpl<std::pair<unsigned, SMLoc> >::const_iterator
           I = Regs.begin(), E = Regs.end(); I != E; ++I)
      Op->Registers.push_back(I->first);
    array_pod_sort(Op->Registers.begin(), Op->Registers.end());
    Op->StartLoc = StartLoc;
    Op->EndLoc = EndLoc;
    return Op;
  }

  static ARMOperand *CreateVectorIndex(unsigned Idx, SMLoc S, SMLoc E,
                                       MCContext &Ctx) {
    ARMOperand *Op = new ARMOperand(k_VectorIndex);
    Op->VectorIndex.Val = Idx;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateImm(const MCExpr *Val, SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_Immediate);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateFPImm(unsigned Val, SMLoc S, MCContext &Ctx) {
    ARMOperand *Op = new ARMOperand(k_FPImmediate);
    Op->FPImm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateMem(unsigned BaseRegNum,
                               const MCConstantExpr *OffsetImm,
                               unsigned OffsetRegNum,
                               ARM_AM::ShiftOpc ShiftType,
                               unsigned ShiftImm,
                               unsigned Alignment,
                               bool isNegative,
                               SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_Memory);
    Op->Memory.BaseRegNum = BaseRegNum;
    Op->Memory.OffsetImm = OffsetImm;
    Op->Memory.OffsetRegNum = OffsetRegNum;
    Op->Memory.ShiftType = ShiftType;
    Op->Memory.ShiftImm = ShiftImm;
    Op->Memory.Alignment = Alignment;
    Op->Memory.isNegative = isNegative;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreatePostIdxReg(unsigned RegNum, bool isAdd,
                                      ARM_AM::ShiftOpc ShiftTy,
                                      unsigned ShiftImm,
                                      SMLoc S, SMLoc E) {
    ARMOperand *Op = new ARMOperand(k_PostIndexRegister);
    Op->PostIdxReg.RegNum = RegNum;
    Op->PostIdxReg.isAdd = isAdd;
    Op->PostIdxReg.ShiftTy = ShiftTy;
    Op->PostIdxReg.ShiftImm = ShiftImm;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static ARMOperand *CreateMemBarrierOpt(ARM_MB::MemBOpt Opt, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_MemBarrierOpt);
    Op->MBOpt.Val = Opt;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateProcIFlags(ARM_PROC::IFlags IFlags, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_ProcIFlags);
    Op->IFlags.Val = IFlags;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  static ARMOperand *CreateMSRMask(unsigned MMask, SMLoc S) {
    ARMOperand *Op = new ARMOperand(k_MSRMask);
    Op->MMask.Val = MMask;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }
};

} // end anonymous namespace.

void ARMOperand::print(raw_ostream &OS) const {
  switch (Kind) {
  case k_FPImmediate:
    OS << "<fpimm " << getFPImm() << "(" << ARM_AM::getFPImmFloat(getFPImm())
       << ") >";
    break;
  case k_CondCode:
    OS << "<ARMCC::" << ARMCondCodeToString(getCondCode()) << ">";
    break;
  case k_CCOut:
    OS << "<ccout " << getReg() << ">";
    break;
  case k_ITCondMask: {
    static char MaskStr[][6] = { "()", "(t)", "(e)", "(tt)", "(et)", "(te)",
      "(ee)", "(ttt)", "(ett)", "(tet)", "(eet)", "(tte)", "(ete)",
      "(tee)", "(eee)" };
    assert((ITMask.Mask & 0xf) == ITMask.Mask);
    OS << "<it-mask " << MaskStr[ITMask.Mask] << ">";
    break;
  }
  case k_CoprocNum:
    OS << "<coprocessor number: " << getCoproc() << ">";
    break;
  case k_CoprocReg:
    OS << "<coprocessor register: " << getCoproc() << ">";
    break;
  case k_CoprocOption:
    OS << "<coprocessor option: " << CoprocOption.Val << ">";
    break;
  case k_MSRMask:
    OS << "<mask: " << getMSRMask() << ">";
    break;
  case k_Immediate:
    getImm()->print(OS);
    break;
  case k_MemBarrierOpt:
    OS << "<ARM_MB::" << MemBOptToString(getMemBarrierOpt()) << ">";
    break;
  case k_Memory:
    OS << "<memory "
       << " base:" << Memory.BaseRegNum;
    OS << ">";
    break;
  case k_PostIndexRegister:
    OS << "post-idx register " << (PostIdxReg.isAdd ? "" : "-")
       << PostIdxReg.RegNum;
    if (PostIdxReg.ShiftTy != ARM_AM::no_shift)
      OS << ARM_AM::getShiftOpcStr(PostIdxReg.ShiftTy) << " "
         << PostIdxReg.ShiftImm;
    OS << ">";
    break;
  case k_ProcIFlags: {
    OS << "<ARM_PROC::";
    unsigned IFlags = getProcIFlags();
    for (int i=2; i >= 0; --i)
      if (IFlags & (1 << i))
        OS << ARM_PROC::IFlagsToString(1 << i);
    OS << ">";
    break;
  }
  case k_Register:
    OS << "<register " << getReg() << ">";
    break;
  case k_ShifterImmediate:
    OS << "<shift " << (ShifterImm.isASR ? "asr" : "lsl")
       << " #" << ShifterImm.Imm << ">";
    break;
  case k_ShiftedRegister:
    OS << "<so_reg_reg "
       << RegShiftedReg.SrcReg
       << ARM_AM::getShiftOpcStr(ARM_AM::getSORegShOp(RegShiftedReg.ShiftImm))
       << ", " << RegShiftedReg.ShiftReg << ", "
       << ARM_AM::getSORegOffset(RegShiftedReg.ShiftImm)
       << ">";
    break;
  case k_ShiftedImmediate:
    OS << "<so_reg_imm "
       << RegShiftedImm.SrcReg
       << ARM_AM::getShiftOpcStr(ARM_AM::getSORegShOp(RegShiftedImm.ShiftImm))
       << ", " << ARM_AM::getSORegOffset(RegShiftedImm.ShiftImm)
       << ">";
    break;
  case k_RotateImmediate:
    OS << "<ror " << " #" << (RotImm.Imm * 8) << ">";
    break;
  case k_BitfieldDescriptor:
    OS << "<bitfield " << "lsb: " << Bitfield.LSB
       << ", width: " << Bitfield.Width << ">";
    break;
  case k_RegisterList:
  case k_DPRRegisterList:
  case k_SPRRegisterList: {
    OS << "<register_list ";

    const SmallVectorImpl<unsigned> &RegList = getRegList();
    for (SmallVectorImpl<unsigned>::const_iterator
           I = RegList.begin(), E = RegList.end(); I != E; ) {
      OS << *I;
      if (++I < E) OS << ", ";
    }

    OS << ">";
    break;
  }
  case k_Token:
    OS << "'" << getToken() << "'";
    break;
  case k_VectorIndex:
    OS << "<vectorindex " << getVectorIndex() << ">";
    break;
  }
}

/// @name Auto-generated Match Functions
/// {

static unsigned MatchRegisterName(StringRef Name);

/// }

bool ARMAsmParser::ParseRegister(unsigned &RegNo,
                                 SMLoc &StartLoc, SMLoc &EndLoc) {
  RegNo = tryParseRegister();

  return (RegNo == (unsigned)-1);
}

/// Try to parse a register name.  The token must be an Identifier when called,
/// and if it is a register name the token is eaten and the register number is
/// returned.  Otherwise return -1.
///
int ARMAsmParser::tryParseRegister() {
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier)) return -1;

  // FIXME: Validate register for the current architecture; we have to do
  // validation later, so maybe there is no need for this here.
  std::string upperCase = Tok.getString().str();
  std::string lowerCase = LowercaseString(upperCase);
  unsigned RegNum = MatchRegisterName(lowerCase);
  if (!RegNum) {
    RegNum = StringSwitch<unsigned>(lowerCase)
      .Case("r13", ARM::SP)
      .Case("r14", ARM::LR)
      .Case("r15", ARM::PC)
      .Case("ip", ARM::R12)
      .Default(0);
  }
  if (!RegNum) return -1;

  Parser.Lex(); // Eat identifier token.

#if 0
  // Also check for an index operand. This is only legal for vector registers,
  // but that'll get caught OK in operand matching, so we don't need to
  // explicitly filter everything else out here.
  if (Parser.getTok().is(AsmToken::LBrac)) {
    SMLoc SIdx = Parser.getTok().getLoc();
    Parser.Lex(); // Eat left bracket token.

    const MCExpr *ImmVal;
    SMLoc ExprLoc = Parser.getTok().getLoc();
    if (getParser().ParseExpression(ImmVal))
      return MatchOperand_ParseFail;
    const MCConstantExpr *MCE = dyn_cast<MCConstantExpr>(ImmVal);
    if (!MCE) {
      TokError("immediate value expected for vector index");
      return MatchOperand_ParseFail;
    }

    SMLoc E = Parser.getTok().getLoc();
    if (Parser.getTok().isNot(AsmToken::RBrac)) {
      Error(E, "']' expected");
      return MatchOperand_ParseFail;
    }

    Parser.Lex(); // Eat right bracket token.

    Operands.push_back(ARMOperand::CreateVectorIndex(MCE->getValue(),
                                                     SIdx, E,
                                                     getContext()));
  }
#endif

  return RegNum;
}

// Try to parse a shifter  (e.g., "lsl <amt>"). On success, return 0.
// If a recoverable error occurs, return 1. If an irrecoverable error
// occurs, return -1. An irrecoverable error is one where tokens have been
// consumed in the process of trying to parse the shifter (i.e., when it is
// indeed a shifter operand, but malformed).
int ARMAsmParser::tryParseShiftRegister(
                               SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  assert(Tok.is(AsmToken::Identifier) && "Token is not an Identifier");

  std::string upperCase = Tok.getString().str();
  std::string lowerCase = LowercaseString(upperCase);
  ARM_AM::ShiftOpc ShiftTy = StringSwitch<ARM_AM::ShiftOpc>(lowerCase)
      .Case("lsl", ARM_AM::lsl)
      .Case("lsr", ARM_AM::lsr)
      .Case("asr", ARM_AM::asr)
      .Case("ror", ARM_AM::ror)
      .Case("rrx", ARM_AM::rrx)
      .Default(ARM_AM::no_shift);

  if (ShiftTy == ARM_AM::no_shift)
    return 1;

  Parser.Lex(); // Eat the operator.

  // The source register for the shift has already been added to the
  // operand list, so we need to pop it off and combine it into the shifted
  // register operand instead.
  OwningPtr<ARMOperand> PrevOp((ARMOperand*)Operands.pop_back_val());
  if (!PrevOp->isReg())
    return Error(PrevOp->getStartLoc(), "shift must be of a register");
  int SrcReg = PrevOp->getReg();
  int64_t Imm = 0;
  int ShiftReg = 0;
  if (ShiftTy == ARM_AM::rrx) {
    // RRX Doesn't have an explicit shift amount. The encoder expects
    // the shift register to be the same as the source register. Seems odd,
    // but OK.
    ShiftReg = SrcReg;
  } else {
    // Figure out if this is shifted by a constant or a register (for non-RRX).
    if (Parser.getTok().is(AsmToken::Hash)) {
      Parser.Lex(); // Eat hash.
      SMLoc ImmLoc = Parser.getTok().getLoc();
      const MCExpr *ShiftExpr = 0;
      if (getParser().ParseExpression(ShiftExpr)) {
        Error(ImmLoc, "invalid immediate shift value");
        return -1;
      }
      // The expression must be evaluatable as an immediate.
      const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(ShiftExpr);
      if (!CE) {
        Error(ImmLoc, "invalid immediate shift value");
        return -1;
      }
      // Range check the immediate.
      // lsl, ror: 0 <= imm <= 31
      // lsr, asr: 0 <= imm <= 32
      Imm = CE->getValue();
      if (Imm < 0 ||
          ((ShiftTy == ARM_AM::lsl || ShiftTy == ARM_AM::ror) && Imm > 31) ||
          ((ShiftTy == ARM_AM::lsr || ShiftTy == ARM_AM::asr) && Imm > 32)) {
        Error(ImmLoc, "immediate shift value out of range");
        return -1;
      }
    } else if (Parser.getTok().is(AsmToken::Identifier)) {
      ShiftReg = tryParseRegister();
      SMLoc L = Parser.getTok().getLoc();
      if (ShiftReg == -1) {
        Error (L, "expected immediate or register in shift operand");
        return -1;
      }
    } else {
      Error (Parser.getTok().getLoc(),
                    "expected immediate or register in shift operand");
      return -1;
    }
  }

  if (ShiftReg && ShiftTy != ARM_AM::rrx)
    Operands.push_back(ARMOperand::CreateShiftedRegister(ShiftTy, SrcReg,
                                                         ShiftReg, Imm,
                                               S, Parser.getTok().getLoc()));
  else
    Operands.push_back(ARMOperand::CreateShiftedImmediate(ShiftTy, SrcReg, Imm,
                                               S, Parser.getTok().getLoc()));

  return 0;
}


/// Try to parse a register name.  The token must be an Identifier when called.
/// If it's a register, an AsmOperand is created. Another AsmOperand is created
/// if there is a "writeback". 'true' if it's not a register.
///
/// TODO this is likely to change to allow different register types and or to
/// parse for a specific register type.
bool ARMAsmParser::
tryParseRegisterWithWriteBack(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  int RegNo = tryParseRegister();
  if (RegNo == -1)
    return true;

  Operands.push_back(ARMOperand::CreateReg(RegNo, S, Parser.getTok().getLoc()));

  const AsmToken &ExclaimTok = Parser.getTok();
  if (ExclaimTok.is(AsmToken::Exclaim)) {
    Operands.push_back(ARMOperand::CreateToken(ExclaimTok.getString(),
                                               ExclaimTok.getLoc()));
    Parser.Lex(); // Eat exclaim token
    return false;
  }

  // Also check for an index operand. This is only legal for vector registers,
  // but that'll get caught OK in operand matching, so we don't need to
  // explicitly filter everything else out here.
  if (Parser.getTok().is(AsmToken::LBrac)) {
    SMLoc SIdx = Parser.getTok().getLoc();
    Parser.Lex(); // Eat left bracket token.

    const MCExpr *ImmVal;
    SMLoc ExprLoc = Parser.getTok().getLoc();
    if (getParser().ParseExpression(ImmVal))
      return MatchOperand_ParseFail;
    const MCConstantExpr *MCE = dyn_cast<MCConstantExpr>(ImmVal);
    if (!MCE) {
      TokError("immediate value expected for vector index");
      return MatchOperand_ParseFail;
    }

    SMLoc E = Parser.getTok().getLoc();
    if (Parser.getTok().isNot(AsmToken::RBrac)) {
      Error(E, "']' expected");
      return MatchOperand_ParseFail;
    }

    Parser.Lex(); // Eat right bracket token.

    Operands.push_back(ARMOperand::CreateVectorIndex(MCE->getValue(),
                                                     SIdx, E,
                                                     getContext()));
  }

  return false;
}

/// MatchCoprocessorOperandName - Try to parse an coprocessor related
/// instruction with a symbolic operand name. Example: "p1", "p7", "c3",
/// "c5", ...
static int MatchCoprocessorOperandName(StringRef Name, char CoprocOp) {
  // Use the same layout as the tablegen'erated register name matcher. Ugly,
  // but efficient.
  switch (Name.size()) {
  default: break;
  case 2:
    if (Name[0] != CoprocOp)
      return -1;
    switch (Name[1]) {
    default:  return -1;
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    }
    break;
  case 3:
    if (Name[0] != CoprocOp || Name[1] != '1')
      return -1;
    switch (Name[2]) {
    default:  return -1;
    case '0': return 10;
    case '1': return 11;
    case '2': return 12;
    case '3': return 13;
    case '4': return 14;
    case '5': return 15;
    }
    break;
  }

  return -1;
}

/// parseITCondCode - Try to parse a condition code for an IT instruction.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseITCondCode(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  if (!Tok.is(AsmToken::Identifier))
    return MatchOperand_NoMatch;
  unsigned CC = StringSwitch<unsigned>(Tok.getString())
    .Case("eq", ARMCC::EQ)
    .Case("ne", ARMCC::NE)
    .Case("hs", ARMCC::HS)
    .Case("cs", ARMCC::HS)
    .Case("lo", ARMCC::LO)
    .Case("cc", ARMCC::LO)
    .Case("mi", ARMCC::MI)
    .Case("pl", ARMCC::PL)
    .Case("vs", ARMCC::VS)
    .Case("vc", ARMCC::VC)
    .Case("hi", ARMCC::HI)
    .Case("ls", ARMCC::LS)
    .Case("ge", ARMCC::GE)
    .Case("lt", ARMCC::LT)
    .Case("gt", ARMCC::GT)
    .Case("le", ARMCC::LE)
    .Case("al", ARMCC::AL)
    .Default(~0U);
  if (CC == ~0U)
    return MatchOperand_NoMatch;
  Parser.Lex(); // Eat the token.

  Operands.push_back(ARMOperand::CreateCondCode(ARMCC::CondCodes(CC), S));

  return MatchOperand_Success;
}

/// parseCoprocNumOperand - Try to parse an coprocessor number operand. The
/// token must be an Identifier when called, and if it is a coprocessor
/// number, the token is eaten and the operand is added to the operand list.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseCoprocNumOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return MatchOperand_NoMatch;

  int Num = MatchCoprocessorOperandName(Tok.getString(), 'p');
  if (Num == -1)
    return MatchOperand_NoMatch;

  Parser.Lex(); // Eat identifier token.
  Operands.push_back(ARMOperand::CreateCoprocNum(Num, S));
  return MatchOperand_Success;
}

/// parseCoprocRegOperand - Try to parse an coprocessor register operand. The
/// token must be an Identifier when called, and if it is a coprocessor
/// number, the token is eaten and the operand is added to the operand list.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseCoprocRegOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return MatchOperand_NoMatch;

  int Reg = MatchCoprocessorOperandName(Tok.getString(), 'c');
  if (Reg == -1)
    return MatchOperand_NoMatch;

  Parser.Lex(); // Eat identifier token.
  Operands.push_back(ARMOperand::CreateCoprocReg(Reg, S));
  return MatchOperand_Success;
}

/// parseCoprocOptionOperand - Try to parse an coprocessor option operand.
/// coproc_option : '{' imm0_255 '}'
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseCoprocOptionOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();

  // If this isn't a '{', this isn't a coprocessor immediate operand.
  if (Parser.getTok().isNot(AsmToken::LCurly))
    return MatchOperand_NoMatch;
  Parser.Lex(); // Eat the '{'

  const MCExpr *Expr;
  SMLoc Loc = Parser.getTok().getLoc();
  if (getParser().ParseExpression(Expr)) {
    Error(Loc, "illegal expression");
    return MatchOperand_ParseFail;
  }
  const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr);
  if (!CE || CE->getValue() < 0 || CE->getValue() > 255) {
    Error(Loc, "coprocessor option must be an immediate in range [0, 255]");
    return MatchOperand_ParseFail;
  }
  int Val = CE->getValue();

  // Check for and consume the closing '}'
  if (Parser.getTok().isNot(AsmToken::RCurly))
    return MatchOperand_ParseFail;
  SMLoc E = Parser.getTok().getLoc();
  Parser.Lex(); // Eat the '}'

  Operands.push_back(ARMOperand::CreateCoprocOption(Val, S, E));
  return MatchOperand_Success;
}

// For register list parsing, we need to map from raw GPR register numbering
// to the enumeration values. The enumeration values aren't sorted by
// register number due to our using "sp", "lr" and "pc" as canonical names.
static unsigned getNextRegister(unsigned Reg) {
  // If this is a GPR, we need to do it manually, otherwise we can rely
  // on the sort ordering of the enumeration since the other reg-classes
  // are sane.
  if (!ARMMCRegisterClasses[ARM::GPRRegClassID].contains(Reg))
    return Reg + 1;
  switch(Reg) {
  default: assert(0 && "Invalid GPR number!");
  case ARM::R0:  return ARM::R1;  case ARM::R1:  return ARM::R2;
  case ARM::R2:  return ARM::R3;  case ARM::R3:  return ARM::R4;
  case ARM::R4:  return ARM::R5;  case ARM::R5:  return ARM::R6;
  case ARM::R6:  return ARM::R7;  case ARM::R7:  return ARM::R8;
  case ARM::R8:  return ARM::R9;  case ARM::R9:  return ARM::R10;
  case ARM::R10: return ARM::R11; case ARM::R11: return ARM::R12;
  case ARM::R12: return ARM::SP;  case ARM::SP:  return ARM::LR;
  case ARM::LR:  return ARM::PC;  case ARM::PC:  return ARM::R0;
  }
}

/// Parse a register list.
bool ARMAsmParser::
parseRegisterList(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  assert(Parser.getTok().is(AsmToken::LCurly) &&
         "Token is not a Left Curly Brace");
  SMLoc S = Parser.getTok().getLoc();
  Parser.Lex(); // Eat '{' token.
  SMLoc RegLoc = Parser.getTok().getLoc();

  // Check the first register in the list to see what register class
  // this is a list of.
  int Reg = tryParseRegister();
  if (Reg == -1)
    return Error(RegLoc, "register expected");

  MCRegisterClass *RC;
  if (ARMMCRegisterClasses[ARM::GPRRegClassID].contains(Reg))
    RC = &ARMMCRegisterClasses[ARM::GPRRegClassID];
  else if (ARMMCRegisterClasses[ARM::DPRRegClassID].contains(Reg))
    RC = &ARMMCRegisterClasses[ARM::DPRRegClassID];
  else if (ARMMCRegisterClasses[ARM::SPRRegClassID].contains(Reg))
    RC = &ARMMCRegisterClasses[ARM::SPRRegClassID];
  else
    return Error(RegLoc, "invalid register in register list");

  // The reglist instructions have at most 16 registers, so reserve
  // space for that many.
  SmallVector<std::pair<unsigned, SMLoc>, 16> Registers;
  // Store the first register.
  Registers.push_back(std::pair<unsigned, SMLoc>(Reg, RegLoc));

  // This starts immediately after the first register token in the list,
  // so we can see either a comma or a minus (range separator) as a legal
  // next token.
  while (Parser.getTok().is(AsmToken::Comma) ||
         Parser.getTok().is(AsmToken::Minus)) {
    if (Parser.getTok().is(AsmToken::Minus)) {
      Parser.Lex(); // Eat the comma.
      SMLoc EndLoc = Parser.getTok().getLoc();
      int EndReg = tryParseRegister();
      if (EndReg == -1)
        return Error(EndLoc, "register expected");
      // If the register is the same as the start reg, there's nothing
      // more to do.
      if (Reg == EndReg)
        continue;
      // The register must be in the same register class as the first.
      if (!RC->contains(EndReg))
        return Error(EndLoc, "invalid register in register list");
      // Ranges must go from low to high.
      if (getARMRegisterNumbering(Reg) > getARMRegisterNumbering(EndReg))
        return Error(EndLoc, "bad range in register list");

      // Add all the registers in the range to the register list.
      while (Reg != EndReg) {
        Reg = getNextRegister(Reg);
        Registers.push_back(std::pair<unsigned, SMLoc>(Reg, RegLoc));
      }
      continue;
    }
    Parser.Lex(); // Eat the comma.
    RegLoc = Parser.getTok().getLoc();
    int OldReg = Reg;
    Reg = tryParseRegister();
    if (Reg == -1)
      return Error(RegLoc, "register expected");
    // The register must be in the same register class as the first.
    if (!RC->contains(Reg))
      return Error(RegLoc, "invalid register in register list");
    // List must be monotonically increasing.
    if (getARMRegisterNumbering(Reg) <= getARMRegisterNumbering(OldReg))
      return Error(RegLoc, "register list not in ascending order");
    // VFP register lists must also be contiguous.
    // It's OK to use the enumeration values directly here rather, as the
    // VFP register classes have the enum sorted properly.
    if (RC != &ARMMCRegisterClasses[ARM::GPRRegClassID] &&
        Reg != OldReg + 1)
      return Error(RegLoc, "non-contiguous register range");
    Registers.push_back(std::pair<unsigned, SMLoc>(Reg, RegLoc));
  }

  SMLoc E = Parser.getTok().getLoc();
  if (Parser.getTok().isNot(AsmToken::RCurly))
    return Error(E, "'}' expected");
  Parser.Lex(); // Eat '}' token.

  Operands.push_back(ARMOperand::CreateRegList(Registers, S, E));
  return false;
}

/// parseMemBarrierOptOperand - Try to parse DSB/DMB data barrier options.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseMemBarrierOptOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  assert(Tok.is(AsmToken::Identifier) && "Token is not an Identifier");
  StringRef OptStr = Tok.getString();

  unsigned Opt = StringSwitch<unsigned>(OptStr.slice(0, OptStr.size()))
    .Case("sy",    ARM_MB::SY)
    .Case("st",    ARM_MB::ST)
    .Case("sh",    ARM_MB::ISH)
    .Case("ish",   ARM_MB::ISH)
    .Case("shst",  ARM_MB::ISHST)
    .Case("ishst", ARM_MB::ISHST)
    .Case("nsh",   ARM_MB::NSH)
    .Case("un",    ARM_MB::NSH)
    .Case("nshst", ARM_MB::NSHST)
    .Case("unst",  ARM_MB::NSHST)
    .Case("osh",   ARM_MB::OSH)
    .Case("oshst", ARM_MB::OSHST)
    .Default(~0U);

  if (Opt == ~0U)
    return MatchOperand_NoMatch;

  Parser.Lex(); // Eat identifier token.
  Operands.push_back(ARMOperand::CreateMemBarrierOpt((ARM_MB::MemBOpt)Opt, S));
  return MatchOperand_Success;
}

/// parseProcIFlagsOperand - Try to parse iflags from CPS instruction.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseProcIFlagsOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  assert(Tok.is(AsmToken::Identifier) && "Token is not an Identifier");
  StringRef IFlagsStr = Tok.getString();

  // An iflags string of "none" is interpreted to mean that none of the AIF
  // bits are set.  Not a terribly useful instruction, but a valid encoding.
  unsigned IFlags = 0;
  if (IFlagsStr != "none") {
        for (int i = 0, e = IFlagsStr.size(); i != e; ++i) {
      unsigned Flag = StringSwitch<unsigned>(IFlagsStr.substr(i, 1))
        .Case("a", ARM_PROC::A)
        .Case("i", ARM_PROC::I)
        .Case("f", ARM_PROC::F)
        .Default(~0U);

      // If some specific iflag is already set, it means that some letter is
      // present more than once, this is not acceptable.
      if (Flag == ~0U || (IFlags & Flag))
        return MatchOperand_NoMatch;

      IFlags |= Flag;
    }
  }

  Parser.Lex(); // Eat identifier token.
  Operands.push_back(ARMOperand::CreateProcIFlags((ARM_PROC::IFlags)IFlags, S));
  return MatchOperand_Success;
}

/// parseMSRMaskOperand - Try to parse mask flags from MSR instruction.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseMSRMaskOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  assert(Tok.is(AsmToken::Identifier) && "Token is not an Identifier");
  StringRef Mask = Tok.getString();

  if (isMClass()) {
    // See ARMv6-M 10.1.1
    unsigned FlagsVal = StringSwitch<unsigned>(Mask)
      .Case("apsr", 0)
      .Case("iapsr", 1)
      .Case("eapsr", 2)
      .Case("xpsr", 3)
      .Case("ipsr", 5)
      .Case("epsr", 6)
      .Case("iepsr", 7)
      .Case("msp", 8)
      .Case("psp", 9)
      .Case("primask", 16)
      .Case("basepri", 17)
      .Case("basepri_max", 18)
      .Case("faultmask", 19)
      .Case("control", 20)
      .Default(~0U);
    
    if (FlagsVal == ~0U)
      return MatchOperand_NoMatch;

    if (!hasV7Ops() && FlagsVal >= 17 && FlagsVal <= 19)
      // basepri, basepri_max and faultmask only valid for V7m.
      return MatchOperand_NoMatch;
    
    Parser.Lex(); // Eat identifier token.
    Operands.push_back(ARMOperand::CreateMSRMask(FlagsVal, S));
    return MatchOperand_Success;
  }

  // Split spec_reg from flag, example: CPSR_sxf => "CPSR" and "sxf"
  size_t Start = 0, Next = Mask.find('_');
  StringRef Flags = "";
  std::string SpecReg = LowercaseString(Mask.slice(Start, Next));
  if (Next != StringRef::npos)
    Flags = Mask.slice(Next+1, Mask.size());

  // FlagsVal contains the complete mask:
  // 3-0: Mask
  // 4: Special Reg (cpsr, apsr => 0; spsr => 1)
  unsigned FlagsVal = 0;

  if (SpecReg == "apsr") {
    FlagsVal = StringSwitch<unsigned>(Flags)
    .Case("nzcvq",  0x8) // same as CPSR_f
    .Case("g",      0x4) // same as CPSR_s
    .Case("nzcvqg", 0xc) // same as CPSR_fs
    .Default(~0U);

    if (FlagsVal == ~0U) {
      if (!Flags.empty())
        return MatchOperand_NoMatch;
      else
        FlagsVal = 8; // No flag
    }
  } else if (SpecReg == "cpsr" || SpecReg == "spsr") {
    if (Flags == "all") // cpsr_all is an alias for cpsr_fc
      Flags = "fc";
    for (int i = 0, e = Flags.size(); i != e; ++i) {
      unsigned Flag = StringSwitch<unsigned>(Flags.substr(i, 1))
      .Case("c", 1)
      .Case("x", 2)
      .Case("s", 4)
      .Case("f", 8)
      .Default(~0U);

      // If some specific flag is already set, it means that some letter is
      // present more than once, this is not acceptable.
      if (FlagsVal == ~0U || (FlagsVal & Flag))
        return MatchOperand_NoMatch;
      FlagsVal |= Flag;
    }
  } else // No match for special register.
    return MatchOperand_NoMatch;

  // Special register without flags are equivalent to "fc" flags.
  if (!FlagsVal)
    FlagsVal = 0x9;

  // Bit 4: Special Reg (cpsr, apsr => 0; spsr => 1)
  if (SpecReg == "spsr")
    FlagsVal |= 16;

  Parser.Lex(); // Eat identifier token.
  Operands.push_back(ARMOperand::CreateMSRMask(FlagsVal, S));
  return MatchOperand_Success;
}

ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parsePKHImm(SmallVectorImpl<MCParsedAsmOperand*> &Operands, StringRef Op,
            int Low, int High) {
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier)) {
    Error(Parser.getTok().getLoc(), Op + " operand expected.");
    return MatchOperand_ParseFail;
  }
  StringRef ShiftName = Tok.getString();
  std::string LowerOp = LowercaseString(Op);
  std::string UpperOp = UppercaseString(Op);
  if (ShiftName != LowerOp && ShiftName != UpperOp) {
    Error(Parser.getTok().getLoc(), Op + " operand expected.");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat shift type token.

  // There must be a '#' and a shift amount.
  if (Parser.getTok().isNot(AsmToken::Hash)) {
    Error(Parser.getTok().getLoc(), "'#' expected");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat hash token.

  const MCExpr *ShiftAmount;
  SMLoc Loc = Parser.getTok().getLoc();
  if (getParser().ParseExpression(ShiftAmount)) {
    Error(Loc, "illegal expression");
    return MatchOperand_ParseFail;
  }
  const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(ShiftAmount);
  if (!CE) {
    Error(Loc, "constant expression expected");
    return MatchOperand_ParseFail;
  }
  int Val = CE->getValue();
  if (Val < Low || Val > High) {
    Error(Loc, "immediate value out of range");
    return MatchOperand_ParseFail;
  }

  Operands.push_back(ARMOperand::CreateImm(CE, Loc, Parser.getTok().getLoc()));

  return MatchOperand_Success;
}

ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseSetEndImm(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  const AsmToken &Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();
  if (Tok.isNot(AsmToken::Identifier)) {
    Error(Tok.getLoc(), "'be' or 'le' operand expected");
    return MatchOperand_ParseFail;
  }
  int Val = StringSwitch<int>(Tok.getString())
    .Case("be", 1)
    .Case("le", 0)
    .Default(-1);
  Parser.Lex(); // Eat the token.

  if (Val == -1) {
    Error(Tok.getLoc(), "'be' or 'le' operand expected");
    return MatchOperand_ParseFail;
  }
  Operands.push_back(ARMOperand::CreateImm(MCConstantExpr::Create(Val,
                                                                  getContext()),
                                           S, Parser.getTok().getLoc()));
  return MatchOperand_Success;
}

/// parseShifterImm - Parse the shifter immediate operand for SSAT/USAT
/// instructions. Legal values are:
///     lsl #n  'n' in [0,31]
///     asr #n  'n' in [1,32]
///             n == 32 encoded as n == 0.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseShifterImm(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  const AsmToken &Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();
  if (Tok.isNot(AsmToken::Identifier)) {
    Error(S, "shift operator 'asr' or 'lsl' expected");
    return MatchOperand_ParseFail;
  }
  StringRef ShiftName = Tok.getString();
  bool isASR;
  if (ShiftName == "lsl" || ShiftName == "LSL")
    isASR = false;
  else if (ShiftName == "asr" || ShiftName == "ASR")
    isASR = true;
  else {
    Error(S, "shift operator 'asr' or 'lsl' expected");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat the operator.

  // A '#' and a shift amount.
  if (Parser.getTok().isNot(AsmToken::Hash)) {
    Error(Parser.getTok().getLoc(), "'#' expected");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat hash token.

  const MCExpr *ShiftAmount;
  SMLoc E = Parser.getTok().getLoc();
  if (getParser().ParseExpression(ShiftAmount)) {
    Error(E, "malformed shift expression");
    return MatchOperand_ParseFail;
  }
  const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(ShiftAmount);
  if (!CE) {
    Error(E, "shift amount must be an immediate");
    return MatchOperand_ParseFail;
  }

  int64_t Val = CE->getValue();
  if (isASR) {
    // Shift amount must be in [1,32]
    if (Val < 1 || Val > 32) {
      Error(E, "'asr' shift amount must be in range [1,32]");
      return MatchOperand_ParseFail;
    }
    // asr #32 encoded as asr #0, but is not allowed in Thumb2 mode.
    if (isThumb() && Val == 32) {
      Error(E, "'asr #32' shift amount not allowed in Thumb mode");
      return MatchOperand_ParseFail;
    }
    if (Val == 32) Val = 0;
  } else {
    // Shift amount must be in [1,32]
    if (Val < 0 || Val > 31) {
      Error(E, "'lsr' shift amount must be in range [0,31]");
      return MatchOperand_ParseFail;
    }
  }

  E = Parser.getTok().getLoc();
  Operands.push_back(ARMOperand::CreateShifterImm(isASR, Val, S, E));

  return MatchOperand_Success;
}

/// parseRotImm - Parse the shifter immediate operand for SXTB/UXTB family
/// of instructions. Legal values are:
///     ror #n  'n' in {0, 8, 16, 24}
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseRotImm(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  const AsmToken &Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();
  if (Tok.isNot(AsmToken::Identifier))
    return MatchOperand_NoMatch;
  StringRef ShiftName = Tok.getString();
  if (ShiftName != "ror" && ShiftName != "ROR")
    return MatchOperand_NoMatch;
  Parser.Lex(); // Eat the operator.

  // A '#' and a rotate amount.
  if (Parser.getTok().isNot(AsmToken::Hash)) {
    Error(Parser.getTok().getLoc(), "'#' expected");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat hash token.

  const MCExpr *ShiftAmount;
  SMLoc E = Parser.getTok().getLoc();
  if (getParser().ParseExpression(ShiftAmount)) {
    Error(E, "malformed rotate expression");
    return MatchOperand_ParseFail;
  }
  const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(ShiftAmount);
  if (!CE) {
    Error(E, "rotate amount must be an immediate");
    return MatchOperand_ParseFail;
  }

  int64_t Val = CE->getValue();
  // Shift amount must be in {0, 8, 16, 24} (0 is undocumented extension)
  // normally, zero is represented in asm by omitting the rotate operand
  // entirely.
  if (Val != 8 && Val != 16 && Val != 24 && Val != 0) {
    Error(E, "'ror' rotate amount must be 8, 16, or 24");
    return MatchOperand_ParseFail;
  }

  E = Parser.getTok().getLoc();
  Operands.push_back(ARMOperand::CreateRotImm(Val, S, E));

  return MatchOperand_Success;
}

ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseBitfield(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  // The bitfield descriptor is really two operands, the LSB and the width.
  if (Parser.getTok().isNot(AsmToken::Hash)) {
    Error(Parser.getTok().getLoc(), "'#' expected");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat hash token.

  const MCExpr *LSBExpr;
  SMLoc E = Parser.getTok().getLoc();
  if (getParser().ParseExpression(LSBExpr)) {
    Error(E, "malformed immediate expression");
    return MatchOperand_ParseFail;
  }
  const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(LSBExpr);
  if (!CE) {
    Error(E, "'lsb' operand must be an immediate");
    return MatchOperand_ParseFail;
  }

  int64_t LSB = CE->getValue();
  // The LSB must be in the range [0,31]
  if (LSB < 0 || LSB > 31) {
    Error(E, "'lsb' operand must be in the range [0,31]");
    return MatchOperand_ParseFail;
  }
  E = Parser.getTok().getLoc();

  // Expect another immediate operand.
  if (Parser.getTok().isNot(AsmToken::Comma)) {
    Error(Parser.getTok().getLoc(), "too few operands");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat hash token.
  if (Parser.getTok().isNot(AsmToken::Hash)) {
    Error(Parser.getTok().getLoc(), "'#' expected");
    return MatchOperand_ParseFail;
  }
  Parser.Lex(); // Eat hash token.

  const MCExpr *WidthExpr;
  if (getParser().ParseExpression(WidthExpr)) {
    Error(E, "malformed immediate expression");
    return MatchOperand_ParseFail;
  }
  CE = dyn_cast<MCConstantExpr>(WidthExpr);
  if (!CE) {
    Error(E, "'width' operand must be an immediate");
    return MatchOperand_ParseFail;
  }

  int64_t Width = CE->getValue();
  // The LSB must be in the range [1,32-lsb]
  if (Width < 1 || Width > 32 - LSB) {
    Error(E, "'width' operand must be in the range [1,32-lsb]");
    return MatchOperand_ParseFail;
  }
  E = Parser.getTok().getLoc();

  Operands.push_back(ARMOperand::CreateBitfield(LSB, Width, S, E));

  return MatchOperand_Success;
}

ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parsePostIdxReg(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Check for a post-index addressing register operand. Specifically:
  // postidx_reg := '+' register {, shift}
  //              | '-' register {, shift}
  //              | register {, shift}

  // This method must return MatchOperand_NoMatch without consuming any tokens
  // in the case where there is no match, as other alternatives take other
  // parse methods.
  AsmToken Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();
  bool haveEaten = false;
  bool isAdd = true;
  int Reg = -1;
  if (Tok.is(AsmToken::Plus)) {
    Parser.Lex(); // Eat the '+' token.
    haveEaten = true;
  } else if (Tok.is(AsmToken::Minus)) {
    Parser.Lex(); // Eat the '-' token.
    isAdd = false;
    haveEaten = true;
  }
  if (Parser.getTok().is(AsmToken::Identifier))
    Reg = tryParseRegister();
  if (Reg == -1) {
    if (!haveEaten)
      return MatchOperand_NoMatch;
    Error(Parser.getTok().getLoc(), "register expected");
    return MatchOperand_ParseFail;
  }
  SMLoc E = Parser.getTok().getLoc();

  ARM_AM::ShiftOpc ShiftTy = ARM_AM::no_shift;
  unsigned ShiftImm = 0;
  if (Parser.getTok().is(AsmToken::Comma)) {
    Parser.Lex(); // Eat the ','.
    if (parseMemRegOffsetShift(ShiftTy, ShiftImm))
      return MatchOperand_ParseFail;
  }

  Operands.push_back(ARMOperand::CreatePostIdxReg(Reg, isAdd, ShiftTy,
                                                  ShiftImm, S, E));

  return MatchOperand_Success;
}

ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseAM3Offset(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Check for a post-index addressing register operand. Specifically:
  // am3offset := '+' register
  //              | '-' register
  //              | register
  //              | # imm
  //              | # + imm
  //              | # - imm

  // This method must return MatchOperand_NoMatch without consuming any tokens
  // in the case where there is no match, as other alternatives take other
  // parse methods.
  AsmToken Tok = Parser.getTok();
  SMLoc S = Tok.getLoc();

  // Do immediates first, as we always parse those if we have a '#'.
  if (Parser.getTok().is(AsmToken::Hash)) {
    Parser.Lex(); // Eat the '#'.
    // Explicitly look for a '-', as we need to encode negative zero
    // differently.
    bool isNegative = Parser.getTok().is(AsmToken::Minus);
    const MCExpr *Offset;
    if (getParser().ParseExpression(Offset))
      return MatchOperand_ParseFail;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Offset);
    if (!CE) {
      Error(S, "constant expression expected");
      return MatchOperand_ParseFail;
    }
    SMLoc E = Tok.getLoc();
    // Negative zero is encoded as the flag value INT32_MIN.
    int32_t Val = CE->getValue();
    if (isNegative && Val == 0)
      Val = INT32_MIN;

    Operands.push_back(
      ARMOperand::CreateImm(MCConstantExpr::Create(Val, getContext()), S, E));

    return MatchOperand_Success;
  }


  bool haveEaten = false;
  bool isAdd = true;
  int Reg = -1;
  if (Tok.is(AsmToken::Plus)) {
    Parser.Lex(); // Eat the '+' token.
    haveEaten = true;
  } else if (Tok.is(AsmToken::Minus)) {
    Parser.Lex(); // Eat the '-' token.
    isAdd = false;
    haveEaten = true;
  }
  if (Parser.getTok().is(AsmToken::Identifier))
    Reg = tryParseRegister();
  if (Reg == -1) {
    if (!haveEaten)
      return MatchOperand_NoMatch;
    Error(Parser.getTok().getLoc(), "register expected");
    return MatchOperand_ParseFail;
  }
  SMLoc E = Parser.getTok().getLoc();

  Operands.push_back(ARMOperand::CreatePostIdxReg(Reg, isAdd, ARM_AM::no_shift,
                                                  0, S, E));

  return MatchOperand_Success;
}

/// cvtT2LdrdPre - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtT2LdrdPre(MCInst &Inst, unsigned Opcode,
             const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Rt, Rt2
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addRegOperands(Inst, 1);
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateReg(0));
  // addr
  ((ARMOperand*)Operands[4])->addMemImm8s4OffsetOperands(Inst, 2);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtT2StrdPre - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtT2StrdPre(MCInst &Inst, unsigned Opcode,
             const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateReg(0));
  // Rt, Rt2
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addRegOperands(Inst, 1);
  // addr
  ((ARMOperand*)Operands[4])->addMemImm8s4OffsetOperands(Inst, 2);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdWriteBackRegT2AddrModeImm8 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdWriteBackRegT2AddrModeImm8(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);

  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));

  ((ARMOperand*)Operands[3])->addMemImm8OffsetOperands(Inst, 2);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtStWriteBackRegT2AddrModeImm8 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStWriteBackRegT2AddrModeImm8(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addMemImm8OffsetOperands(Inst, 2);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdWriteBackRegAddrMode2 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdWriteBackRegAddrMode2(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);

  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));

  ((ARMOperand*)Operands[3])->addAddrMode2Operands(Inst, 3);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdWriteBackRegAddrModeImm12 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdWriteBackRegAddrModeImm12(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);

  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));

  ((ARMOperand*)Operands[3])->addMemImm12OffsetOperands(Inst, 2);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}


/// cvtStWriteBackRegAddrModeImm12 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStWriteBackRegAddrModeImm12(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addMemImm12OffsetOperands(Inst, 2);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtStWriteBackRegAddrMode2 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStWriteBackRegAddrMode2(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addAddrMode2Operands(Inst, 3);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtStWriteBackRegAddrMode3 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStWriteBackRegAddrMode3(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addAddrMode3Operands(Inst, 3);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdExtTWriteBackImm - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdExtTWriteBackImm(MCInst &Inst, unsigned Opcode,
                      const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Rt
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  // addr
  ((ARMOperand*)Operands[3])->addMemNoOffsetOperands(Inst, 1);
  // offset
  ((ARMOperand*)Operands[4])->addPostIdxImm8Operands(Inst, 1);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdExtTWriteBackReg - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdExtTWriteBackReg(MCInst &Inst, unsigned Opcode,
                      const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Rt
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  // addr
  ((ARMOperand*)Operands[3])->addMemNoOffsetOperands(Inst, 1);
  // offset
  ((ARMOperand*)Operands[4])->addPostIdxRegOperands(Inst, 2);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtStExtTWriteBackImm - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStExtTWriteBackImm(MCInst &Inst, unsigned Opcode,
                      const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  // Rt
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  // addr
  ((ARMOperand*)Operands[3])->addMemNoOffsetOperands(Inst, 1);
  // offset
  ((ARMOperand*)Operands[4])->addPostIdxImm8Operands(Inst, 1);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtStExtTWriteBackReg - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStExtTWriteBackReg(MCInst &Inst, unsigned Opcode,
                      const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  // Rt
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  // addr
  ((ARMOperand*)Operands[3])->addMemNoOffsetOperands(Inst, 1);
  // offset
  ((ARMOperand*)Operands[4])->addPostIdxRegOperands(Inst, 2);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdrdPre - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdrdPre(MCInst &Inst, unsigned Opcode,
           const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Rt, Rt2
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addRegOperands(Inst, 1);
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  // addr
  ((ARMOperand*)Operands[4])->addAddrMode3Operands(Inst, 3);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtStrdPre - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtStrdPre(MCInst &Inst, unsigned Opcode,
           const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  // Rt, Rt2
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[3])->addRegOperands(Inst, 1);
  // addr
  ((ARMOperand*)Operands[4])->addAddrMode3Operands(Inst, 3);
  // pred
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtLdWriteBackRegAddrMode3 - Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtLdWriteBackRegAddrMode3(MCInst &Inst, unsigned Opcode,
                         const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  ((ARMOperand*)Operands[2])->addRegOperands(Inst, 1);
  // Create a writeback register dummy placeholder.
  Inst.addOperand(MCOperand::CreateImm(0));
  ((ARMOperand*)Operands[3])->addAddrMode3Operands(Inst, 3);
  ((ARMOperand*)Operands[1])->addCondCodeOperands(Inst, 2);
  return true;
}

/// cvtThumbMultiple- Convert parsed operands to MCInst.
/// Needed here because the Asm Gen Matcher can't handle properly tied operands
/// when they refer multiple MIOperands inside a single one.
bool ARMAsmParser::
cvtThumbMultiply(MCInst &Inst, unsigned Opcode,
           const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // The second source operand must be the same register as the destination
  // operand.
  if (Operands.size() == 6 &&
      (((ARMOperand*)Operands[3])->getReg() !=
       ((ARMOperand*)Operands[5])->getReg()) &&
      (((ARMOperand*)Operands[3])->getReg() !=
       ((ARMOperand*)Operands[4])->getReg())) {
    Error(Operands[3]->getStartLoc(),
          "destination register must match source register");
    return false;
  }
  ((ARMOperand*)Operands[3])->addRegOperands(Inst, 1);
  ((ARMOperand*)Operands[1])->addCCOutOperands(Inst, 1);
  ((ARMOperand*)Operands[4])->addRegOperands(Inst, 1);
  // If we have a three-operand form, use that, else the second source operand
  // is just the destination operand again.
  if (Operands.size() == 6)
    ((ARMOperand*)Operands[5])->addRegOperands(Inst, 1);
  else
    Inst.addOperand(Inst.getOperand(0));
  ((ARMOperand*)Operands[2])->addCondCodeOperands(Inst, 2);

  return true;
}

/// Parse an ARM memory expression, return false if successful else return true
/// or an error.  The first token must be a '[' when called.
bool ARMAsmParser::
parseMemory(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S, E;
  assert(Parser.getTok().is(AsmToken::LBrac) &&
         "Token is not a Left Bracket");
  S = Parser.getTok().getLoc();
  Parser.Lex(); // Eat left bracket token.

  const AsmToken &BaseRegTok = Parser.getTok();
  int BaseRegNum = tryParseRegister();
  if (BaseRegNum == -1)
    return Error(BaseRegTok.getLoc(), "register expected");

  // The next token must either be a comma or a closing bracket.
  const AsmToken &Tok = Parser.getTok();
  if (!Tok.is(AsmToken::Comma) && !Tok.is(AsmToken::RBrac))
    return Error(Tok.getLoc(), "malformed memory operand");

  if (Tok.is(AsmToken::RBrac)) {
    E = Tok.getLoc();
    Parser.Lex(); // Eat right bracket token.

    Operands.push_back(ARMOperand::CreateMem(BaseRegNum, 0, 0, ARM_AM::no_shift,
                                             0, 0, false, S, E));

    // If there's a pre-indexing writeback marker, '!', just add it as a token
    // operand. It's rather odd, but syntactically valid.
    if (Parser.getTok().is(AsmToken::Exclaim)) {
      Operands.push_back(ARMOperand::CreateToken("!",Parser.getTok().getLoc()));
      Parser.Lex(); // Eat the '!'.
    }

    return false;
  }

  assert(Tok.is(AsmToken::Comma) && "Lost comma in memory operand?!");
  Parser.Lex(); // Eat the comma.

  // If we have a ':', it's an alignment specifier.
  if (Parser.getTok().is(AsmToken::Colon)) {
    Parser.Lex(); // Eat the ':'.
    E = Parser.getTok().getLoc();

    const MCExpr *Expr;
    if (getParser().ParseExpression(Expr))
     return true;

    // The expression has to be a constant. Memory references with relocations
    // don't come through here, as they use the <label> forms of the relevant
    // instructions.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr);
    if (!CE)
      return Error (E, "constant expression expected");

    unsigned Align = 0;
    switch (CE->getValue()) {
    default:
      return Error(E, "alignment specifier must be 64, 128, or 256 bits");
    case 64:  Align = 8; break;
    case 128: Align = 16; break;
    case 256: Align = 32; break;
    }

    // Now we should have the closing ']'
    E = Parser.getTok().getLoc();
    if (Parser.getTok().isNot(AsmToken::RBrac))
      return Error(E, "']' expected");
    Parser.Lex(); // Eat right bracket token.

    // Don't worry about range checking the value here. That's handled by
    // the is*() predicates.
    Operands.push_back(ARMOperand::CreateMem(BaseRegNum, 0, 0,
                                             ARM_AM::no_shift, 0, Align,
                                             false, S, E));

    // If there's a pre-indexing writeback marker, '!', just add it as a token
    // operand.
    if (Parser.getTok().is(AsmToken::Exclaim)) {
      Operands.push_back(ARMOperand::CreateToken("!",Parser.getTok().getLoc()));
      Parser.Lex(); // Eat the '!'.
    }

    return false;
  }

  // If we have a '#', it's an immediate offset, else assume it's a register
  // offset.
  if (Parser.getTok().is(AsmToken::Hash)) {
    Parser.Lex(); // Eat the '#'.
    E = Parser.getTok().getLoc();

    bool isNegative = getParser().getTok().is(AsmToken::Minus);
    const MCExpr *Offset;
    if (getParser().ParseExpression(Offset))
     return true;

    // The expression has to be a constant. Memory references with relocations
    // don't come through here, as they use the <label> forms of the relevant
    // instructions.
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Offset);
    if (!CE)
      return Error (E, "constant expression expected");

    // If the constant was #-0, represent it as INT32_MIN.
    int32_t Val = CE->getValue();
    if (isNegative && Val == 0)
      CE = MCConstantExpr::Create(INT32_MIN, getContext());

    // Now we should have the closing ']'
    E = Parser.getTok().getLoc();
    if (Parser.getTok().isNot(AsmToken::RBrac))
      return Error(E, "']' expected");
    Parser.Lex(); // Eat right bracket token.

    // Don't worry about range checking the value here. That's handled by
    // the is*() predicates.
    Operands.push_back(ARMOperand::CreateMem(BaseRegNum, CE, 0,
                                             ARM_AM::no_shift, 0, 0,
                                             false, S, E));

    // If there's a pre-indexing writeback marker, '!', just add it as a token
    // operand.
    if (Parser.getTok().is(AsmToken::Exclaim)) {
      Operands.push_back(ARMOperand::CreateToken("!",Parser.getTok().getLoc()));
      Parser.Lex(); // Eat the '!'.
    }

    return false;
  }

  // The register offset is optionally preceded by a '+' or '-'
  bool isNegative = false;
  if (Parser.getTok().is(AsmToken::Minus)) {
    isNegative = true;
    Parser.Lex(); // Eat the '-'.
  } else if (Parser.getTok().is(AsmToken::Plus)) {
    // Nothing to do.
    Parser.Lex(); // Eat the '+'.
  }

  E = Parser.getTok().getLoc();
  int OffsetRegNum = tryParseRegister();
  if (OffsetRegNum == -1)
    return Error(E, "register expected");

  // If there's a shift operator, handle it.
  ARM_AM::ShiftOpc ShiftType = ARM_AM::no_shift;
  unsigned ShiftImm = 0;
  if (Parser.getTok().is(AsmToken::Comma)) {
    Parser.Lex(); // Eat the ','.
    if (parseMemRegOffsetShift(ShiftType, ShiftImm))
      return true;
  }

  // Now we should have the closing ']'
  E = Parser.getTok().getLoc();
  if (Parser.getTok().isNot(AsmToken::RBrac))
    return Error(E, "']' expected");
  Parser.Lex(); // Eat right bracket token.

  Operands.push_back(ARMOperand::CreateMem(BaseRegNum, 0, OffsetRegNum,
                                           ShiftType, ShiftImm, 0, isNegative,
                                           S, E));

  // If there's a pre-indexing writeback marker, '!', just add it as a token
  // operand.
  if (Parser.getTok().is(AsmToken::Exclaim)) {
    Operands.push_back(ARMOperand::CreateToken("!",Parser.getTok().getLoc()));
    Parser.Lex(); // Eat the '!'.
  }

  return false;
}

/// parseMemRegOffsetShift - one of these two:
///   ( lsl | lsr | asr | ror ) , # shift_amount
///   rrx
/// return true if it parses a shift otherwise it returns false.
bool ARMAsmParser::parseMemRegOffsetShift(ARM_AM::ShiftOpc &St,
                                          unsigned &Amount) {
  SMLoc Loc = Parser.getTok().getLoc();
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return true;
  StringRef ShiftName = Tok.getString();
  if (ShiftName == "lsl" || ShiftName == "LSL")
    St = ARM_AM::lsl;
  else if (ShiftName == "lsr" || ShiftName == "LSR")
    St = ARM_AM::lsr;
  else if (ShiftName == "asr" || ShiftName == "ASR")
    St = ARM_AM::asr;
  else if (ShiftName == "ror" || ShiftName == "ROR")
    St = ARM_AM::ror;
  else if (ShiftName == "rrx" || ShiftName == "RRX")
    St = ARM_AM::rrx;
  else
    return Error(Loc, "illegal shift operator");
  Parser.Lex(); // Eat shift type token.

  // rrx stands alone.
  Amount = 0;
  if (St != ARM_AM::rrx) {
    Loc = Parser.getTok().getLoc();
    // A '#' and a shift amount.
    const AsmToken &HashTok = Parser.getTok();
    if (HashTok.isNot(AsmToken::Hash))
      return Error(HashTok.getLoc(), "'#' expected");
    Parser.Lex(); // Eat hash token.

    const MCExpr *Expr;
    if (getParser().ParseExpression(Expr))
      return true;
    // Range check the immediate.
    // lsl, ror: 0 <= imm <= 31
    // lsr, asr: 0 <= imm <= 32
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr);
    if (!CE)
      return Error(Loc, "shift amount must be an immediate");
    int64_t Imm = CE->getValue();
    if (Imm < 0 ||
        ((St == ARM_AM::lsl || St == ARM_AM::ror) && Imm > 31) ||
        ((St == ARM_AM::lsr || St == ARM_AM::asr) && Imm > 32))
      return Error(Loc, "immediate shift value out of range");
    Amount = Imm;
  }

  return false;
}

/// parseFPImm - A floating point immediate expression operand.
ARMAsmParser::OperandMatchResultTy ARMAsmParser::
parseFPImm(SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  SMLoc S = Parser.getTok().getLoc();

  if (Parser.getTok().isNot(AsmToken::Hash))
    return MatchOperand_NoMatch;
  Parser.Lex(); // Eat the '#'.

  // Handle negation, as that still comes through as a separate token.
  bool isNegative = false;
  if (Parser.getTok().is(AsmToken::Minus)) {
    isNegative = true;
    Parser.Lex();
  }
  const AsmToken &Tok = Parser.getTok();
  if (Tok.is(AsmToken::Real)) {
    APFloat RealVal(APFloat::IEEEdouble, Tok.getString());
    uint64_t IntVal = RealVal.bitcastToAPInt().getZExtValue();
    // If we had a '-' in front, toggle the sign bit.
    IntVal ^= (uint64_t)isNegative << 63;
    int Val = ARM_AM::getFP64Imm(APInt(64, IntVal));
    Parser.Lex(); // Eat the token.
    if (Val == -1) {
      TokError("floating point value out of range");
      return MatchOperand_ParseFail;
    }
    Operands.push_back(ARMOperand::CreateFPImm(Val, S, getContext()));
    return MatchOperand_Success;
  }
  if (Tok.is(AsmToken::Integer)) {
    int64_t Val = Tok.getIntVal();
    Parser.Lex(); // Eat the token.
    if (Val > 255 || Val < 0) {
      TokError("encoded floating point value out of range");
      return MatchOperand_ParseFail;
    }
    Operands.push_back(ARMOperand::CreateFPImm(Val, S, getContext()));
    return MatchOperand_Success;
  }

  TokError("invalid floating point immediate");
  return MatchOperand_ParseFail;
}
/// Parse a arm instruction operand.  For now this parses the operand regardless
/// of the mnemonic.
bool ARMAsmParser::parseOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                                StringRef Mnemonic) {
  SMLoc S, E;

  // Check if the current operand has a custom associated parser, if so, try to
  // custom parse the operand, or fallback to the general approach.
  OperandMatchResultTy ResTy = MatchOperandParserImpl(Operands, Mnemonic);
  if (ResTy == MatchOperand_Success)
    return false;
  // If there wasn't a custom match, try the generic matcher below. Otherwise,
  // there was a match, but an error occurred, in which case, just return that
  // the operand parsing failed.
  if (ResTy == MatchOperand_ParseFail)
    return true;

  switch (getLexer().getKind()) {
  default:
    Error(Parser.getTok().getLoc(), "unexpected token in operand");
    return true;
  case AsmToken::Identifier: {
    // If this is VMRS, check for the apsr_nzcv operand.
    if (!tryParseRegisterWithWriteBack(Operands))
      return false;
    int Res = tryParseShiftRegister(Operands);
    if (Res == 0) // success
      return false;
    else if (Res == -1) // irrecoverable error
      return true;
    if (Mnemonic == "vmrs" && Parser.getTok().getString() == "apsr_nzcv") {
      S = Parser.getTok().getLoc();
      Parser.Lex();
      Operands.push_back(ARMOperand::CreateToken("apsr_nzcv", S));
      return false;
    }

    // Fall though for the Identifier case that is not a register or a
    // special name.
  }
  case AsmToken::Integer: // things like 1f and 2b as a branch targets
  case AsmToken::Dot: {   // . as a branch target
    // This was not a register so parse other operands that start with an
    // identifier (like labels) as expressions and create them as immediates.
    const MCExpr *IdVal;
    S = Parser.getTok().getLoc();
    if (getParser().ParseExpression(IdVal))
      return true;
    E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    Operands.push_back(ARMOperand::CreateImm(IdVal, S, E));
    return false;
  }
  case AsmToken::LBrac:
    return parseMemory(Operands);
  case AsmToken::LCurly:
    return parseRegisterList(Operands);
  case AsmToken::Hash: {
    // #42 -> immediate.
    // TODO: ":lower16:" and ":upper16:" modifiers after # before immediate
    S = Parser.getTok().getLoc();
    Parser.Lex();
    bool isNegative = Parser.getTok().is(AsmToken::Minus);
    const MCExpr *ImmVal;
    if (getParser().ParseExpression(ImmVal))
      return true;
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(ImmVal);
    if (!CE) {
      Error(S, "constant expression expected");
      return MatchOperand_ParseFail;
    }
    int32_t Val = CE->getValue();
    if (isNegative && Val == 0)
      ImmVal = MCConstantExpr::Create(INT32_MIN, getContext());
    E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    Operands.push_back(ARMOperand::CreateImm(ImmVal, S, E));
    return false;
  }
  case AsmToken::Colon: {
    // ":lower16:" and ":upper16:" expression prefixes
    // FIXME: Check it's an expression prefix,
    // e.g. (FOO - :lower16:BAR) isn't legal.
    ARMMCExpr::VariantKind RefKind;
    if (parsePrefix(RefKind))
      return true;

    const MCExpr *SubExprVal;
    if (getParser().ParseExpression(SubExprVal))
      return true;

    const MCExpr *ExprVal = ARMMCExpr::Create(RefKind, SubExprVal,
                                                   getContext());
    E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    Operands.push_back(ARMOperand::CreateImm(ExprVal, S, E));
    return false;
  }
  }
}

// parsePrefix - Parse ARM 16-bit relocations expression prefix, i.e.
//  :lower16: and :upper16:.
bool ARMAsmParser::parsePrefix(ARMMCExpr::VariantKind &RefKind) {
  RefKind = ARMMCExpr::VK_ARM_None;

  // :lower16: and :upper16: modifiers
  assert(getLexer().is(AsmToken::Colon) && "expected a :");
  Parser.Lex(); // Eat ':'

  if (getLexer().isNot(AsmToken::Identifier)) {
    Error(Parser.getTok().getLoc(), "expected prefix identifier in operand");
    return true;
  }

  StringRef IDVal = Parser.getTok().getIdentifier();
  if (IDVal == "lower16") {
    RefKind = ARMMCExpr::VK_ARM_LO16;
  } else if (IDVal == "upper16") {
    RefKind = ARMMCExpr::VK_ARM_HI16;
  } else {
    Error(Parser.getTok().getLoc(), "unexpected prefix in operand");
    return true;
  }
  Parser.Lex();

  if (getLexer().isNot(AsmToken::Colon)) {
    Error(Parser.getTok().getLoc(), "unexpected token after prefix");
    return true;
  }
  Parser.Lex(); // Eat the last ':'
  return false;
}

/// \brief Given a mnemonic, split out possible predication code and carry
/// setting letters to form a canonical mnemonic and flags.
//
// FIXME: Would be nice to autogen this.
// FIXME: This is a bit of a maze of special cases.
StringRef ARMAsmParser::splitMnemonic(StringRef Mnemonic,
                                      unsigned &PredicationCode,
                                      bool &CarrySetting,
                                      unsigned &ProcessorIMod,
                                      StringRef &ITMask) {
  PredicationCode = ARMCC::AL;
  CarrySetting = false;
  ProcessorIMod = 0;

  // Ignore some mnemonics we know aren't predicated forms.
  //
  // FIXME: Would be nice to autogen this.
  if ((Mnemonic == "movs" && isThumb()) ||
      Mnemonic == "teq"   || Mnemonic == "vceq"   || Mnemonic == "svc"   ||
      Mnemonic == "mls"   || Mnemonic == "smmls"  || Mnemonic == "vcls"  ||
      Mnemonic == "vmls"  || Mnemonic == "vnmls"  || Mnemonic == "vacge" ||
      Mnemonic == "vcge"  || Mnemonic == "vclt"   || Mnemonic == "vacgt" ||
      Mnemonic == "vcgt"  || Mnemonic == "vcle"   || Mnemonic == "smlal" ||
      Mnemonic == "umaal" || Mnemonic == "umlal"  || Mnemonic == "vabal" ||
      Mnemonic == "vmlal" || Mnemonic == "vpadal" || Mnemonic == "vqdmlal")
    return Mnemonic;

  // First, split out any predication code. Ignore mnemonics we know aren't
  // predicated but do have a carry-set and so weren't caught above.
  if (Mnemonic != "adcs" && Mnemonic != "bics" && Mnemonic != "movs" &&
      Mnemonic != "muls" && Mnemonic != "smlals" && Mnemonic != "smulls" &&
      Mnemonic != "umlals" && Mnemonic != "umulls" && Mnemonic != "lsls" &&
      Mnemonic != "sbcs" && Mnemonic != "rscs") {
    unsigned CC = StringSwitch<unsigned>(Mnemonic.substr(Mnemonic.size()-2))
      .Case("eq", ARMCC::EQ)
      .Case("ne", ARMCC::NE)
      .Case("hs", ARMCC::HS)
      .Case("cs", ARMCC::HS)
      .Case("lo", ARMCC::LO)
      .Case("cc", ARMCC::LO)
      .Case("mi", ARMCC::MI)
      .Case("pl", ARMCC::PL)
      .Case("vs", ARMCC::VS)
      .Case("vc", ARMCC::VC)
      .Case("hi", ARMCC::HI)
      .Case("ls", ARMCC::LS)
      .Case("ge", ARMCC::GE)
      .Case("lt", ARMCC::LT)
      .Case("gt", ARMCC::GT)
      .Case("le", ARMCC::LE)
      .Case("al", ARMCC::AL)
      .Default(~0U);
    if (CC != ~0U) {
      Mnemonic = Mnemonic.slice(0, Mnemonic.size() - 2);
      PredicationCode = CC;
    }
  }

  // Next, determine if we have a carry setting bit. We explicitly ignore all
  // the instructions we know end in 's'.
  if (Mnemonic.endswith("s") &&
      !(Mnemonic == "cps" || Mnemonic == "mls" ||
        Mnemonic == "mrs" || Mnemonic == "smmls" || Mnemonic == "vabs" ||
        Mnemonic == "vcls" || Mnemonic == "vmls" || Mnemonic == "vmrs" ||
        Mnemonic == "vnmls" || Mnemonic == "vqabs" || Mnemonic == "vrecps" ||
        Mnemonic == "vrsqrts" || Mnemonic == "srs" ||
        (Mnemonic == "movs" && isThumb()))) {
    Mnemonic = Mnemonic.slice(0, Mnemonic.size() - 1);
    CarrySetting = true;
  }

  // The "cps" instruction can have a interrupt mode operand which is glued into
  // the mnemonic. Check if this is the case, split it and parse the imod op
  if (Mnemonic.startswith("cps")) {
    // Split out any imod code.
    unsigned IMod =
      StringSwitch<unsigned>(Mnemonic.substr(Mnemonic.size()-2, 2))
      .Case("ie", ARM_PROC::IE)
      .Case("id", ARM_PROC::ID)
      .Default(~0U);
    if (IMod != ~0U) {
      Mnemonic = Mnemonic.slice(0, Mnemonic.size()-2);
      ProcessorIMod = IMod;
    }
  }

  // The "it" instruction has the condition mask on the end of the mnemonic.
  if (Mnemonic.startswith("it")) {
    ITMask = Mnemonic.slice(2, Mnemonic.size());
    Mnemonic = Mnemonic.slice(0, 2);
  }

  return Mnemonic;
}

/// \brief Given a canonical mnemonic, determine if the instruction ever allows
/// inclusion of carry set or predication code operands.
//
// FIXME: It would be nice to autogen this.
void ARMAsmParser::
getMnemonicAcceptInfo(StringRef Mnemonic, bool &CanAcceptCarrySet,
                      bool &CanAcceptPredicationCode) {
  if (Mnemonic == "and" || Mnemonic == "lsl" || Mnemonic == "lsr" ||
      Mnemonic == "rrx" || Mnemonic == "ror" || Mnemonic == "sub" ||
      Mnemonic == "add" || Mnemonic == "adc" ||
      Mnemonic == "mul" || Mnemonic == "bic" || Mnemonic == "asr" ||
      Mnemonic == "orr" || Mnemonic == "mvn" ||
      Mnemonic == "rsb" || Mnemonic == "rsc" || Mnemonic == "orn" ||
      Mnemonic == "sbc" || Mnemonic == "eor" || Mnemonic == "neg" ||
      (!isThumb() && (Mnemonic == "smull" || Mnemonic == "mov" ||
                      Mnemonic == "mla" || Mnemonic == "smlal" ||
                      Mnemonic == "umlal" || Mnemonic == "umull"))) {
    CanAcceptCarrySet = true;
  } else
    CanAcceptCarrySet = false;

  if (Mnemonic == "cbnz" || Mnemonic == "setend" || Mnemonic == "dmb" ||
      Mnemonic == "cps" || Mnemonic == "mcr2" || Mnemonic == "it" ||
      Mnemonic == "mcrr2" || Mnemonic == "cbz" || Mnemonic == "cdp2" ||
      Mnemonic == "trap" || Mnemonic == "mrc2" || Mnemonic == "mrrc2" ||
      Mnemonic == "dsb" || Mnemonic == "isb" || Mnemonic == "setend" ||
      (Mnemonic == "clrex" && !isThumb()) ||
      (Mnemonic == "nop" && isThumbOne()) ||
      ((Mnemonic == "pld" || Mnemonic == "pli" || Mnemonic == "pldw" ||
        Mnemonic == "ldc2" || Mnemonic == "ldc2l" ||
        Mnemonic == "stc2" || Mnemonic == "stc2l") && !isThumb()) ||
      ((Mnemonic.startswith("rfe") || Mnemonic.startswith("srs")) &&
       !isThumb()) ||
      Mnemonic.startswith("cps") || (Mnemonic == "movs" && isThumbOne())) {
    CanAcceptPredicationCode = false;
  } else
    CanAcceptPredicationCode = true;

  if (isThumb()) {
    if (Mnemonic == "bkpt" || Mnemonic == "mcr" || Mnemonic == "mcrr" ||
        Mnemonic == "mrc" || Mnemonic == "mrrc" || Mnemonic == "cdp")
      CanAcceptPredicationCode = false;
  }
}

bool ARMAsmParser::shouldOmitCCOutOperand(StringRef Mnemonic,
                               SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // FIXME: This is all horribly hacky. We really need a better way to deal
  // with optional operands like this in the matcher table.

  // The 'mov' mnemonic is special. One variant has a cc_out operand, while
  // another does not. Specifically, the MOVW instruction does not. So we
  // special case it here and remove the defaulted (non-setting) cc_out
  // operand if that's the instruction we're trying to match.
  //
  // We do this as post-processing of the explicit operands rather than just
  // conditionally adding the cc_out in the first place because we need
  // to check the type of the parsed immediate operand.
  if (Mnemonic == "mov" && Operands.size() > 4 && !isThumb() &&
      !static_cast<ARMOperand*>(Operands[4])->isARMSOImm() &&
      static_cast<ARMOperand*>(Operands[4])->isImm0_65535Expr() &&
      static_cast<ARMOperand*>(Operands[1])->getReg() == 0)
    return true;

  // Register-register 'add' for thumb does not have a cc_out operand
  // when there are only two register operands.
  if (isThumb() && Mnemonic == "add" && Operands.size() == 5 &&
      static_cast<ARMOperand*>(Operands[3])->isReg() &&
      static_cast<ARMOperand*>(Operands[4])->isReg() &&
      static_cast<ARMOperand*>(Operands[1])->getReg() == 0)
    return true;
  // Register-register 'add' for thumb does not have a cc_out operand
  // when it's an ADD Rdm, SP, {Rdm|#imm0_255} instruction. We do
  // have to check the immediate range here since Thumb2 has a variant
  // that can handle a different range and has a cc_out operand.
  if (((isThumb() && Mnemonic == "add") ||
       (isThumbTwo() && Mnemonic == "sub")) &&
      Operands.size() == 6 &&
      static_cast<ARMOperand*>(Operands[3])->isReg() &&
      static_cast<ARMOperand*>(Operands[4])->isReg() &&
      static_cast<ARMOperand*>(Operands[4])->getReg() == ARM::SP &&
      static_cast<ARMOperand*>(Operands[1])->getReg() == 0 &&
      (static_cast<ARMOperand*>(Operands[5])->isReg() ||
       static_cast<ARMOperand*>(Operands[5])->isImm0_1020s4()))
    return true;
  // For Thumb2, add/sub immediate does not have a cc_out operand for the
  // imm0_4095 variant. That's the least-preferred variant when
  // selecting via the generic "add" mnemonic, so to know that we
  // should remove the cc_out operand, we have to explicitly check that
  // it's not one of the other variants. Ugh.
  if (isThumbTwo() && (Mnemonic == "add" || Mnemonic == "sub") &&
      Operands.size() == 6 &&
      static_cast<ARMOperand*>(Operands[3])->isReg() &&
      static_cast<ARMOperand*>(Operands[4])->isReg() &&
      static_cast<ARMOperand*>(Operands[5])->isImm()) {
    // Nest conditions rather than one big 'if' statement for readability.
    //
    // If either register is a high reg, it's either one of the SP
    // variants (handled above) or a 32-bit encoding, so we just
    // check against T3.
    if ((!isARMLowRegister(static_cast<ARMOperand*>(Operands[3])->getReg()) ||
         !isARMLowRegister(static_cast<ARMOperand*>(Operands[4])->getReg())) &&
        static_cast<ARMOperand*>(Operands[5])->isT2SOImm())
      return false;
    // If both registers are low, we're in an IT block, and the immediate is
    // in range, we should use encoding T1 instead, which has a cc_out.
    if (inITBlock() &&
        isARMLowRegister(static_cast<ARMOperand*>(Operands[3])->getReg()) &&
        isARMLowRegister(static_cast<ARMOperand*>(Operands[4])->getReg()) &&
        static_cast<ARMOperand*>(Operands[5])->isImm0_7())
      return false;

    // Otherwise, we use encoding T4, which does not have a cc_out
    // operand.
    return true;
  }

  // The thumb2 multiply instruction doesn't have a CCOut register, so
  // if we have a "mul" mnemonic in Thumb mode, check if we'll be able to
  // use the 16-bit encoding or not.
  if (isThumbTwo() && Mnemonic == "mul" && Operands.size() == 6 &&
      static_cast<ARMOperand*>(Operands[1])->getReg() == 0 &&
      static_cast<ARMOperand*>(Operands[3])->isReg() &&
      static_cast<ARMOperand*>(Operands[4])->isReg() &&
      static_cast<ARMOperand*>(Operands[5])->isReg() &&
      // If the registers aren't low regs, the destination reg isn't the
      // same as one of the source regs, or the cc_out operand is zero
      // outside of an IT block, we have to use the 32-bit encoding, so
      // remove the cc_out operand.
      (!isARMLowRegister(static_cast<ARMOperand*>(Operands[3])->getReg()) ||
       !isARMLowRegister(static_cast<ARMOperand*>(Operands[4])->getReg()) ||
       !inITBlock() ||
       (static_cast<ARMOperand*>(Operands[3])->getReg() !=
        static_cast<ARMOperand*>(Operands[5])->getReg() &&
        static_cast<ARMOperand*>(Operands[3])->getReg() !=
        static_cast<ARMOperand*>(Operands[4])->getReg())))
    return true;



  // Register-register 'add/sub' for thumb does not have a cc_out operand
  // when it's an ADD/SUB SP, #imm. Be lenient on count since there's also
  // the "add/sub SP, SP, #imm" version. If the follow-up operands aren't
  // right, this will result in better diagnostics (which operand is off)
  // anyway.
  if (isThumb() && (Mnemonic == "add" || Mnemonic == "sub") &&
      (Operands.size() == 5 || Operands.size() == 6) &&
      static_cast<ARMOperand*>(Operands[3])->isReg() &&
      static_cast<ARMOperand*>(Operands[3])->getReg() == ARM::SP &&
      static_cast<ARMOperand*>(Operands[1])->getReg() == 0)
    return true;

  return false;
}

/// Parse an arm instruction mnemonic followed by its operands.
bool ARMAsmParser::ParseInstruction(StringRef Name, SMLoc NameLoc,
                               SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  // Create the leading tokens for the mnemonic, split by '.' characters.
  size_t Start = 0, Next = Name.find('.');
  StringRef Mnemonic = Name.slice(Start, Next);

  // Split out the predication code and carry setting flag from the mnemonic.
  unsigned PredicationCode;
  unsigned ProcessorIMod;
  bool CarrySetting;
  StringRef ITMask;
  Mnemonic = splitMnemonic(Mnemonic, PredicationCode, CarrySetting,
                           ProcessorIMod, ITMask);

  // In Thumb1, only the branch (B) instruction can be predicated.
  if (isThumbOne() && PredicationCode != ARMCC::AL && Mnemonic != "b") {
    Parser.EatToEndOfStatement();
    return Error(NameLoc, "conditional execution not supported in Thumb1");
  }

  Operands.push_back(ARMOperand::CreateToken(Mnemonic, NameLoc));

  // Handle the IT instruction ITMask. Convert it to a bitmask. This
  // is the mask as it will be for the IT encoding if the conditional
  // encoding has a '1' as it's bit0 (i.e. 't' ==> '1'). In the case
  // where the conditional bit0 is zero, the instruction post-processing
  // will adjust the mask accordingly.
  if (Mnemonic == "it") {
    SMLoc Loc = SMLoc::getFromPointer(NameLoc.getPointer() + 2);
    if (ITMask.size() > 3) {
      Parser.EatToEndOfStatement();
      return Error(Loc, "too many conditions on IT instruction");
    }
    unsigned Mask = 8;
    for (unsigned i = ITMask.size(); i != 0; --i) {
      char pos = ITMask[i - 1];
      if (pos != 't' && pos != 'e') {
        Parser.EatToEndOfStatement();
        return Error(Loc, "illegal IT block condition mask '" + ITMask + "'");
      }
      Mask >>= 1;
      if (ITMask[i - 1] == 't')
        Mask |= 8;
    }
    Operands.push_back(ARMOperand::CreateITMask(Mask, Loc));
  }

  // FIXME: This is all a pretty gross hack. We should automatically handle
  // optional operands like this via tblgen.

  // Next, add the CCOut and ConditionCode operands, if needed.
  //
  // For mnemonics which can ever incorporate a carry setting bit or predication
  // code, our matching model involves us always generating CCOut and
  // ConditionCode operands to match the mnemonic "as written" and then we let
  // the matcher deal with finding the right instruction or generating an
  // appropriate error.
  bool CanAcceptCarrySet, CanAcceptPredicationCode;
  getMnemonicAcceptInfo(Mnemonic, CanAcceptCarrySet, CanAcceptPredicationCode);

  // If we had a carry-set on an instruction that can't do that, issue an
  // error.
  if (!CanAcceptCarrySet && CarrySetting) {
    Parser.EatToEndOfStatement();
    return Error(NameLoc, "instruction '" + Mnemonic +
                 "' can not set flags, but 's' suffix specified");
  }
  // If we had a predication code on an instruction that can't do that, issue an
  // error.
  if (!CanAcceptPredicationCode && PredicationCode != ARMCC::AL) {
    Parser.EatToEndOfStatement();
    return Error(NameLoc, "instruction '" + Mnemonic +
                 "' is not predicable, but condition code specified");
  }

  // Add the carry setting operand, if necessary.
  if (CanAcceptCarrySet) {
    SMLoc Loc = SMLoc::getFromPointer(NameLoc.getPointer() + Mnemonic.size());
    Operands.push_back(ARMOperand::CreateCCOut(CarrySetting ? ARM::CPSR : 0,
                                               Loc));
  }

  // Add the predication code operand, if necessary.
  if (CanAcceptPredicationCode) {
    SMLoc Loc = SMLoc::getFromPointer(NameLoc.getPointer() + Mnemonic.size() +
                                      CarrySetting);
    Operands.push_back(ARMOperand::CreateCondCode(
                         ARMCC::CondCodes(PredicationCode), Loc));
  }

  // Add the processor imod operand, if necessary.
  if (ProcessorIMod) {
    Operands.push_back(ARMOperand::CreateImm(
          MCConstantExpr::Create(ProcessorIMod, getContext()),
                                 NameLoc, NameLoc));
  }

  // Add the remaining tokens in the mnemonic.
  while (Next != StringRef::npos) {
    Start = Next;
    Next = Name.find('.', Start + 1);
    StringRef ExtraToken = Name.slice(Start, Next);

    // For now, we're only parsing Thumb1 (for the most part), so
    // just ignore ".n" qualifiers. We'll use them to restrict
    // matching when we do Thumb2.
    if (ExtraToken != ".n") {
      SMLoc Loc = SMLoc::getFromPointer(NameLoc.getPointer() + Start);
      Operands.push_back(ARMOperand::CreateToken(ExtraToken, Loc));
    }
  }

  // Read the remaining operands.
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    // Read the first operand.
    if (parseOperand(Operands, Mnemonic)) {
      Parser.EatToEndOfStatement();
      return true;
    }

    while (getLexer().is(AsmToken::Comma)) {
      Parser.Lex();  // Eat the comma.

      // Parse and remember the operand.
      if (parseOperand(Operands, Mnemonic)) {
        Parser.EatToEndOfStatement();
        return true;
      }
    }
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.EatToEndOfStatement();
    return Error(Loc, "unexpected token in argument list");
  }

  Parser.Lex(); // Consume the EndOfStatement

  // Some instructions, mostly Thumb, have forms for the same mnemonic that
  // do and don't have a cc_out optional-def operand. With some spot-checks
  // of the operand list, we can figure out which variant we're trying to
  // parse and adjust accordingly before actually matching. We shouldn't ever
  // try to remove a cc_out operand that was explicitly set on the the
  // mnemonic, of course (CarrySetting == true). Reason number #317 the
  // table driven matcher doesn't fit well with the ARM instruction set.
  if (!CarrySetting && shouldOmitCCOutOperand(Mnemonic, Operands)) {
    ARMOperand *Op = static_cast<ARMOperand*>(Operands[1]);
    Operands.erase(Operands.begin() + 1);
    delete Op;
  }

  // ARM mode 'blx' need special handling, as the register operand version
  // is predicable, but the label operand version is not. So, we can't rely
  // on the Mnemonic based checking to correctly figure out when to put
  // a k_CondCode operand in the list. If we're trying to match the label
  // version, remove the k_CondCode operand here.
  if (!isThumb() && Mnemonic == "blx" && Operands.size() == 3 &&
      static_cast<ARMOperand*>(Operands[2])->isImm()) {
    ARMOperand *Op = static_cast<ARMOperand*>(Operands[1]);
    Operands.erase(Operands.begin() + 1);
    delete Op;
  }

  // The vector-compare-to-zero instructions have a literal token "#0" at
  // the end that comes to here as an immediate operand. Convert it to a
  // token to play nicely with the matcher.
  if ((Mnemonic == "vceq" || Mnemonic == "vcge" || Mnemonic == "vcgt" ||
      Mnemonic == "vcle" || Mnemonic == "vclt") && Operands.size() == 6 &&
      static_cast<ARMOperand*>(Operands[5])->isImm()) {
    ARMOperand *Op = static_cast<ARMOperand*>(Operands[5]);
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Op->getImm());
    if (CE && CE->getValue() == 0) {
      Operands.erase(Operands.begin() + 5);
      Operands.push_back(ARMOperand::CreateToken("#0", Op->getStartLoc()));
      delete Op;
    }
  }
  // VCMP{E} does the same thing, but with a different operand count.
  if ((Mnemonic == "vcmp" || Mnemonic == "vcmpe") && Operands.size() == 5 &&
      static_cast<ARMOperand*>(Operands[4])->isImm()) {
    ARMOperand *Op = static_cast<ARMOperand*>(Operands[4]);
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Op->getImm());
    if (CE && CE->getValue() == 0) {
      Operands.erase(Operands.begin() + 4);
      Operands.push_back(ARMOperand::CreateToken("#0", Op->getStartLoc()));
      delete Op;
    }
  }
  // Similarly, the Thumb1 "RSB" instruction has a literal "#0" on the
  // end. Convert it to a token here.
  if (Mnemonic == "rsb" && isThumb() && Operands.size() == 6 &&
      static_cast<ARMOperand*>(Operands[5])->isImm()) {
    ARMOperand *Op = static_cast<ARMOperand*>(Operands[5]);
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Op->getImm());
    if (CE && CE->getValue() == 0) {
      Operands.erase(Operands.begin() + 5);
      Operands.push_back(ARMOperand::CreateToken("#0", Op->getStartLoc()));
      delete Op;
    }
  }

  return false;
}

// Validate context-sensitive operand constraints.

// return 'true' if register list contains non-low GPR registers,
// 'false' otherwise. If Reg is in the register list or is HiReg, set
// 'containsReg' to true.
static bool checkLowRegisterList(MCInst Inst, unsigned OpNo, unsigned Reg,
                                 unsigned HiReg, bool &containsReg) {
  containsReg = false;
  for (unsigned i = OpNo; i < Inst.getNumOperands(); ++i) {
    unsigned OpReg = Inst.getOperand(i).getReg();
    if (OpReg == Reg)
      containsReg = true;
    // Anything other than a low register isn't legal here.
    if (!isARMLowRegister(OpReg) && (!HiReg || OpReg != HiReg))
      return true;
  }
  return false;
}

// Check if the specified regisgter is in the register list of the inst,
// starting at the indicated operand number.
static bool listContainsReg(MCInst &Inst, unsigned OpNo, unsigned Reg) {
  for (unsigned i = OpNo; i < Inst.getNumOperands(); ++i) {
    unsigned OpReg = Inst.getOperand(i).getReg();
    if (OpReg == Reg)
      return true;
  }
  return false;
}

// FIXME: We would really prefer to have MCInstrInfo (the wrapper around
// the ARMInsts array) instead. Getting that here requires awkward
// API changes, though. Better way?
namespace llvm {
extern MCInstrDesc ARMInsts[];
}
static MCInstrDesc &getInstDesc(unsigned Opcode) {
  return ARMInsts[Opcode];
}

// FIXME: We would really like to be able to tablegen'erate this.
bool ARMAsmParser::
validateInstruction(MCInst &Inst,
                    const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  MCInstrDesc &MCID = getInstDesc(Inst.getOpcode());
  SMLoc Loc = Operands[0]->getStartLoc();
  // Check the IT block state first.
  // NOTE: In Thumb mode, the BKPT instruction has the interesting property of
  // being allowed in IT blocks, but not being predicable.  It just always
  // executes.
  if (inITBlock() && Inst.getOpcode() != ARM::tBKPT) {
    unsigned bit = 1;
    if (ITState.FirstCond)
      ITState.FirstCond = false;
    else
      bit = (ITState.Mask >> (5 - ITState.CurPosition)) & 1;
    // The instruction must be predicable.
    if (!MCID.isPredicable())
      return Error(Loc, "instructions in IT block must be predicable");
    unsigned Cond = Inst.getOperand(MCID.findFirstPredOperandIdx()).getImm();
    unsigned ITCond = bit ? ITState.Cond :
      ARMCC::getOppositeCondition(ITState.Cond);
    if (Cond != ITCond) {
      // Find the condition code Operand to get its SMLoc information.
      SMLoc CondLoc;
      for (unsigned i = 1; i < Operands.size(); ++i)
        if (static_cast<ARMOperand*>(Operands[i])->isCondCode())
          CondLoc = Operands[i]->getStartLoc();
      return Error(CondLoc, "incorrect condition in IT block; got '" +
                   StringRef(ARMCondCodeToString(ARMCC::CondCodes(Cond))) +
                   "', but expected '" +
                   ARMCondCodeToString(ARMCC::CondCodes(ITCond)) + "'");
    }
  // Check for non-'al' condition codes outside of the IT block.
  } else if (isThumbTwo() && MCID.isPredicable() &&
             Inst.getOperand(MCID.findFirstPredOperandIdx()).getImm() !=
             ARMCC::AL && Inst.getOpcode() != ARM::tB &&
             Inst.getOpcode() != ARM::t2B)
    return Error(Loc, "predicated instructions must be in IT block");

  switch (Inst.getOpcode()) {
  case ARM::LDRD:
  case ARM::LDRD_PRE:
  case ARM::LDRD_POST:
  case ARM::LDREXD: {
    // Rt2 must be Rt + 1.
    unsigned Rt = getARMRegisterNumbering(Inst.getOperand(0).getReg());
    unsigned Rt2 = getARMRegisterNumbering(Inst.getOperand(1).getReg());
    if (Rt2 != Rt + 1)
      return Error(Operands[3]->getStartLoc(),
                   "destination operands must be sequential");
    return false;
  }
  case ARM::STRD: {
    // Rt2 must be Rt + 1.
    unsigned Rt = getARMRegisterNumbering(Inst.getOperand(0).getReg());
    unsigned Rt2 = getARMRegisterNumbering(Inst.getOperand(1).getReg());
    if (Rt2 != Rt + 1)
      return Error(Operands[3]->getStartLoc(),
                   "source operands must be sequential");
    return false;
  }
  case ARM::STRD_PRE:
  case ARM::STRD_POST:
  case ARM::STREXD: {
    // Rt2 must be Rt + 1.
    unsigned Rt = getARMRegisterNumbering(Inst.getOperand(1).getReg());
    unsigned Rt2 = getARMRegisterNumbering(Inst.getOperand(2).getReg());
    if (Rt2 != Rt + 1)
      return Error(Operands[3]->getStartLoc(),
                   "source operands must be sequential");
    return false;
  }
  case ARM::SBFX:
  case ARM::UBFX: {
    // width must be in range [1, 32-lsb]
    unsigned lsb = Inst.getOperand(2).getImm();
    unsigned widthm1 = Inst.getOperand(3).getImm();
    if (widthm1 >= 32 - lsb)
      return Error(Operands[5]->getStartLoc(),
                   "bitfield width must be in range [1,32-lsb]");
    return false;
  }
  case ARM::tLDMIA: {
    // If we're parsing Thumb2, the .w variant is available and handles
    // most cases that are normally illegal for a Thumb1 LDM
    // instruction. We'll make the transformation in processInstruction()
    // if necessary.
    //
    // Thumb LDM instructions are writeback iff the base register is not
    // in the register list.
    unsigned Rn = Inst.getOperand(0).getReg();
    bool hasWritebackToken =
      (static_cast<ARMOperand*>(Operands[3])->isToken() &&
       static_cast<ARMOperand*>(Operands[3])->getToken() == "!");
    bool listContainsBase;
    if (checkLowRegisterList(Inst, 3, Rn, 0, listContainsBase) && !isThumbTwo())
      return Error(Operands[3 + hasWritebackToken]->getStartLoc(),
                   "registers must be in range r0-r7");
    // If we should have writeback, then there should be a '!' token.
    if (!listContainsBase && !hasWritebackToken && !isThumbTwo())
      return Error(Operands[2]->getStartLoc(),
                   "writeback operator '!' expected");
    // If we should not have writeback, there must not be a '!'. This is
    // true even for the 32-bit wide encodings.
    if (listContainsBase && hasWritebackToken)
      return Error(Operands[3]->getStartLoc(),
                   "writeback operator '!' not allowed when base register "
                   "in register list");

    break;
  }
  case ARM::t2LDMIA_UPD: {
    if (listContainsReg(Inst, 3, Inst.getOperand(0).getReg()))
      return Error(Operands[4]->getStartLoc(),
                   "writeback operator '!' not allowed when base register "
                   "in register list");
    break;
  }
  case ARM::tPOP: {
    bool listContainsBase;
    if (checkLowRegisterList(Inst, 3, 0, ARM::PC, listContainsBase))
      return Error(Operands[2]->getStartLoc(),
                   "registers must be in range r0-r7 or pc");
    break;
  }
  case ARM::tPUSH: {
    bool listContainsBase;
    if (checkLowRegisterList(Inst, 3, 0, ARM::LR, listContainsBase))
      return Error(Operands[2]->getStartLoc(),
                   "registers must be in range r0-r7 or lr");
    break;
  }
  case ARM::tSTMIA_UPD: {
    bool listContainsBase;
    if (checkLowRegisterList(Inst, 4, 0, 0, listContainsBase) && !isThumbTwo())
      return Error(Operands[4]->getStartLoc(),
                   "registers must be in range r0-r7");
    break;
  }
  }

  return false;
}

void ARMAsmParser::
processInstruction(MCInst &Inst,
                   const SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  switch (Inst.getOpcode()) {
  case ARM::LDMIA_UPD:
    // If this is a load of a single register via a 'pop', then we should use
    // a post-indexed LDR instruction instead, per the ARM ARM.
    if (static_cast<ARMOperand*>(Operands[0])->getToken() == "pop" &&
        Inst.getNumOperands() == 5) {
      MCInst TmpInst;
      TmpInst.setOpcode(ARM::LDR_POST_IMM);
      TmpInst.addOperand(Inst.getOperand(4)); // Rt
      TmpInst.addOperand(Inst.getOperand(0)); // Rn_wb
      TmpInst.addOperand(Inst.getOperand(1)); // Rn
      TmpInst.addOperand(MCOperand::CreateReg(0));  // am2offset
      TmpInst.addOperand(MCOperand::CreateImm(4));
      TmpInst.addOperand(Inst.getOperand(2)); // CondCode
      TmpInst.addOperand(Inst.getOperand(3));
      Inst = TmpInst;
    }
    break;
  case ARM::STMDB_UPD:
    // If this is a store of a single register via a 'push', then we should use
    // a pre-indexed STR instruction instead, per the ARM ARM.
    if (static_cast<ARMOperand*>(Operands[0])->getToken() == "push" &&
        Inst.getNumOperands() == 5) {
      MCInst TmpInst;
      TmpInst.setOpcode(ARM::STR_PRE_IMM);
      TmpInst.addOperand(Inst.getOperand(0)); // Rn_wb
      TmpInst.addOperand(Inst.getOperand(4)); // Rt
      TmpInst.addOperand(Inst.getOperand(1)); // addrmode_imm12
      TmpInst.addOperand(MCOperand::CreateImm(-4));
      TmpInst.addOperand(Inst.getOperand(2)); // CondCode
      TmpInst.addOperand(Inst.getOperand(3));
      Inst = TmpInst;
    }
    break;
  case ARM::tADDi8:
    // If the immediate is in the range 0-7, we want tADDi3 iff Rd was
    // explicitly specified. From the ARM ARM: "Encoding T1 is preferred
    // to encoding T2 if <Rd> is specified and encoding T2 is preferred
    // to encoding T1 if <Rd> is omitted."
    if (Inst.getOperand(3).getImm() < 8 && Operands.size() == 6)
      Inst.setOpcode(ARM::tADDi3);
    break;
  case ARM::tSUBi8:
    // If the immediate is in the range 0-7, we want tADDi3 iff Rd was
    // explicitly specified. From the ARM ARM: "Encoding T1 is preferred
    // to encoding T2 if <Rd> is specified and encoding T2 is preferred
    // to encoding T1 if <Rd> is omitted."
    if (Inst.getOperand(3).getImm() < 8 && Operands.size() == 6)
      Inst.setOpcode(ARM::tSUBi3);
    break;
  case ARM::tB:
    // A Thumb conditional branch outside of an IT block is a tBcc.
    if (Inst.getOperand(1).getImm() != ARMCC::AL && !inITBlock())
      Inst.setOpcode(ARM::tBcc);
    break;
  case ARM::t2B:
    // A Thumb2 conditional branch outside of an IT block is a t2Bcc.
    if (Inst.getOperand(1).getImm() != ARMCC::AL && !inITBlock())
      Inst.setOpcode(ARM::t2Bcc);
    break;
  case ARM::t2Bcc:
    // If the conditional is AL or we're in an IT block, we really want t2B.
    if (Inst.getOperand(1).getImm() == ARMCC::AL || inITBlock())
      Inst.setOpcode(ARM::t2B);
    break;
  case ARM::tBcc:
    // If the conditional is AL, we really want tB.
    if (Inst.getOperand(1).getImm() == ARMCC::AL)
      Inst.setOpcode(ARM::tB);
    break;
  case ARM::tLDMIA: {
    // If the register list contains any high registers, or if the writeback
    // doesn't match what tLDMIA can do, we need to use the 32-bit encoding
    // instead if we're in Thumb2. Otherwise, this should have generated
    // an error in validateInstruction().
    unsigned Rn = Inst.getOperand(0).getReg();
    bool hasWritebackToken =
      (static_cast<ARMOperand*>(Operands[3])->isToken() &&
       static_cast<ARMOperand*>(Operands[3])->getToken() == "!");
    bool listContainsBase;
    if (checkLowRegisterList(Inst, 3, Rn, 0, listContainsBase) ||
        (!listContainsBase && !hasWritebackToken) ||
        (listContainsBase && hasWritebackToken)) {
      // 16-bit encoding isn't sufficient. Switch to the 32-bit version.
      assert (isThumbTwo());
      Inst.setOpcode(hasWritebackToken ? ARM::t2LDMIA_UPD : ARM::t2LDMIA);
      // If we're switching to the updating version, we need to insert
      // the writeback tied operand.
      if (hasWritebackToken)
        Inst.insert(Inst.begin(),
                    MCOperand::CreateReg(Inst.getOperand(0).getReg()));
    }
    break;
  }
  case ARM::tSTMIA_UPD: {
    // If the register list contains any high registers, we need to use
    // the 32-bit encoding instead if we're in Thumb2. Otherwise, this
    // should have generated an error in validateInstruction().
    unsigned Rn = Inst.getOperand(0).getReg();
    bool listContainsBase;
    if (checkLowRegisterList(Inst, 4, Rn, 0, listContainsBase)) {
      // 16-bit encoding isn't sufficient. Switch to the 32-bit version.
      assert (isThumbTwo());
      Inst.setOpcode(ARM::t2STMIA_UPD);
    }
    break;
  }
  case ARM::t2MOVi: {
    // If we can use the 16-bit encoding and the user didn't explicitly
    // request the 32-bit variant, transform it here.
    if (isARMLowRegister(Inst.getOperand(0).getReg()) &&
        Inst.getOperand(1).getImm() <= 255 &&
        ((!inITBlock() && Inst.getOperand(2).getImm() == ARMCC::AL &&
         Inst.getOperand(4).getReg() == ARM::CPSR) ||
        (inITBlock() && Inst.getOperand(4).getReg() == 0)) &&
        (!static_cast<ARMOperand*>(Operands[2])->isToken() ||
         static_cast<ARMOperand*>(Operands[2])->getToken() != ".w")) {
      // The operands aren't in the same order for tMOVi8...
      MCInst TmpInst;
      TmpInst.setOpcode(ARM::tMOVi8);
      TmpInst.addOperand(Inst.getOperand(0));
      TmpInst.addOperand(Inst.getOperand(4));
      TmpInst.addOperand(Inst.getOperand(1));
      TmpInst.addOperand(Inst.getOperand(2));
      TmpInst.addOperand(Inst.getOperand(3));
      Inst = TmpInst;
    }
    break;
  }
  case ARM::t2MOVr: {
    // If we can use the 16-bit encoding and the user didn't explicitly
    // request the 32-bit variant, transform it here.
    if (isARMLowRegister(Inst.getOperand(0).getReg()) &&
        isARMLowRegister(Inst.getOperand(1).getReg()) &&
        Inst.getOperand(2).getImm() == ARMCC::AL &&
        Inst.getOperand(4).getReg() == ARM::CPSR &&
        (!static_cast<ARMOperand*>(Operands[2])->isToken() ||
         static_cast<ARMOperand*>(Operands[2])->getToken() != ".w")) {
      // The operands aren't the same for tMOV[S]r... (no cc_out)
      MCInst TmpInst;
      TmpInst.setOpcode(Inst.getOperand(4).getReg() ? ARM::tMOVSr : ARM::tMOVr);
      TmpInst.addOperand(Inst.getOperand(0));
      TmpInst.addOperand(Inst.getOperand(1));
      TmpInst.addOperand(Inst.getOperand(2));
      TmpInst.addOperand(Inst.getOperand(3));
      Inst = TmpInst;
    }
    break;
  }
  case ARM::t2SXTH:
  case ARM::t2SXTB:
  case ARM::t2UXTH:
  case ARM::t2UXTB: {
    // If we can use the 16-bit encoding and the user didn't explicitly
    // request the 32-bit variant, transform it here.
    if (isARMLowRegister(Inst.getOperand(0).getReg()) &&
        isARMLowRegister(Inst.getOperand(1).getReg()) &&
        Inst.getOperand(2).getImm() == 0 &&
        (!static_cast<ARMOperand*>(Operands[2])->isToken() ||
         static_cast<ARMOperand*>(Operands[2])->getToken() != ".w")) {
      unsigned NewOpc;
      switch (Inst.getOpcode()) {
      default: llvm_unreachable("Illegal opcode!");
      case ARM::t2SXTH: NewOpc = ARM::tSXTH; break;
      case ARM::t2SXTB: NewOpc = ARM::tSXTB; break;
      case ARM::t2UXTH: NewOpc = ARM::tUXTH; break;
      case ARM::t2UXTB: NewOpc = ARM::tUXTB; break;
      }
      // The operands aren't the same for thumb1 (no rotate operand).
      MCInst TmpInst;
      TmpInst.setOpcode(NewOpc);
      TmpInst.addOperand(Inst.getOperand(0));
      TmpInst.addOperand(Inst.getOperand(1));
      TmpInst.addOperand(Inst.getOperand(3));
      TmpInst.addOperand(Inst.getOperand(4));
      Inst = TmpInst;
    }
    break;
  }
  case ARM::t2IT: {
    // The mask bits for all but the first condition are represented as
    // the low bit of the condition code value implies 't'. We currently
    // always have 1 implies 't', so XOR toggle the bits if the low bit
    // of the condition code is zero. The encoding also expects the low
    // bit of the condition to be encoded as bit 4 of the mask operand,
    // so mask that in if needed
    MCOperand &MO = Inst.getOperand(1);
    unsigned Mask = MO.getImm();
    unsigned OrigMask = Mask;
    unsigned TZ = CountTrailingZeros_32(Mask);
    if ((Inst.getOperand(0).getImm() & 1) == 0) {
      assert(Mask && TZ <= 3 && "illegal IT mask value!");
      for (unsigned i = 3; i != TZ; --i)
        Mask ^= 1 << i;
    } else
      Mask |= 0x10;
    MO.setImm(Mask);

    // Set up the IT block state according to the IT instruction we just
    // matched.
    assert(!inITBlock() && "nested IT blocks?!");
    ITState.Cond = ARMCC::CondCodes(Inst.getOperand(0).getImm());
    ITState.Mask = OrigMask; // Use the original mask, not the updated one.
    ITState.CurPosition = 0;
    ITState.FirstCond = true;
    break;
  }
  }
}

unsigned ARMAsmParser::checkTargetMatchPredicate(MCInst &Inst) {
  // 16-bit thumb arithmetic instructions either require or preclude the 'S'
  // suffix depending on whether they're in an IT block or not.
  unsigned Opc = Inst.getOpcode();
  MCInstrDesc &MCID = getInstDesc(Opc);
  if (MCID.TSFlags & ARMII::ThumbArithFlagSetting) {
    assert(MCID.hasOptionalDef() &&
           "optionally flag setting instruction missing optional def operand");
    assert(MCID.NumOperands == Inst.getNumOperands() &&
           "operand count mismatch!");
    // Find the optional-def operand (cc_out).
    unsigned OpNo;
    for (OpNo = 0;
         !MCID.OpInfo[OpNo].isOptionalDef() && OpNo < MCID.NumOperands;
         ++OpNo)
      ;
    // If we're parsing Thumb1, reject it completely.
    if (isThumbOne() && Inst.getOperand(OpNo).getReg() != ARM::CPSR)
      return Match_MnemonicFail;
    // If we're parsing Thumb2, which form is legal depends on whether we're
    // in an IT block.
    if (isThumbTwo() && Inst.getOperand(OpNo).getReg() != ARM::CPSR &&
        !inITBlock())
      return Match_RequiresITBlock;
    if (isThumbTwo() && Inst.getOperand(OpNo).getReg() == ARM::CPSR &&
        inITBlock())
      return Match_RequiresNotITBlock;
  }
  // Some high-register supporting Thumb1 encodings only allow both registers
  // to be from r0-r7 when in Thumb2.
  else if (Opc == ARM::tADDhirr && isThumbOne() &&
           isARMLowRegister(Inst.getOperand(1).getReg()) &&
           isARMLowRegister(Inst.getOperand(2).getReg()))
    return Match_RequiresThumb2;
  // Others only require ARMv6 or later.
  else if (Opc == ARM::tMOVr && isThumbOne() && !hasV6Ops() &&
           isARMLowRegister(Inst.getOperand(0).getReg()) &&
           isARMLowRegister(Inst.getOperand(1).getReg()))
    return Match_RequiresV6;
  return Match_Success;
}

bool ARMAsmParser::
MatchAndEmitInstruction(SMLoc IDLoc,
                        SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                        MCStreamer &Out) {
  MCInst Inst;
  unsigned ErrorInfo;
  unsigned MatchResult;
  MatchResult = MatchInstructionImpl(Operands, Inst, ErrorInfo);
  switch (MatchResult) {
  default: break;
  case Match_Success:
    // Context sensitive operand constraints aren't handled by the matcher,
    // so check them here.
    if (validateInstruction(Inst, Operands)) {
      // Still progress the IT block, otherwise one wrong condition causes
      // nasty cascading errors.
      forwardITPosition();
      return true;
    }

    // Some instructions need post-processing to, for example, tweak which
    // encoding is selected.
    processInstruction(Inst, Operands);

    // Only move forward at the very end so that everything in validate
    // and process gets a consistent answer about whether we're in an IT
    // block.
    forwardITPosition();

    Out.EmitInstruction(Inst);
    return false;
  case Match_MissingFeature:
    Error(IDLoc, "instruction requires a CPU feature not currently enabled");
    return true;
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0U) {
      if (ErrorInfo >= Operands.size())
        return Error(IDLoc, "too few operands for instruction");

      ErrorLoc = ((ARMOperand*)Operands[ErrorInfo])->getStartLoc();
      if (ErrorLoc == SMLoc()) ErrorLoc = IDLoc;
    }

    return Error(ErrorLoc, "invalid operand for instruction");
  }
  case Match_MnemonicFail:
    return Error(IDLoc, "invalid instruction");
  case Match_ConversionFail:
    // The converter function will have already emited a diagnostic.
    return true;
  case Match_RequiresNotITBlock:
    return Error(IDLoc, "flag setting instruction only valid outside IT block");
  case Match_RequiresITBlock:
    return Error(IDLoc, "instruction only valid inside IT block");
  case Match_RequiresV6:
    return Error(IDLoc, "instruction variant requires ARMv6 or later");
  case Match_RequiresThumb2:
    return Error(IDLoc, "instruction variant requires Thumb2");
  }

  llvm_unreachable("Implement any new match types added!");
  return true;
}

/// parseDirective parses the arm specific directives
bool ARMAsmParser::ParseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getIdentifier();
  if (IDVal == ".word")
    return parseDirectiveWord(4, DirectiveID.getLoc());
  else if (IDVal == ".thumb")
    return parseDirectiveThumb(DirectiveID.getLoc());
  else if (IDVal == ".thumb_func")
    return parseDirectiveThumbFunc(DirectiveID.getLoc());
  else if (IDVal == ".code")
    return parseDirectiveCode(DirectiveID.getLoc());
  else if (IDVal == ".syntax")
    return parseDirectiveSyntax(DirectiveID.getLoc());
  return true;
}

/// parseDirectiveWord
///  ::= .word [ expression (, expression)* ]
bool ARMAsmParser::parseDirectiveWord(unsigned Size, SMLoc L) {
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    for (;;) {
      const MCExpr *Value;
      if (getParser().ParseExpression(Value))
        return true;

      getParser().getStreamer().EmitValue(Value, Size, 0/*addrspace*/);

      if (getLexer().is(AsmToken::EndOfStatement))
        break;

      // FIXME: Improve diagnostic.
      if (getLexer().isNot(AsmToken::Comma))
        return Error(L, "unexpected token in directive");
      Parser.Lex();
    }
  }

  Parser.Lex();
  return false;
}

/// parseDirectiveThumb
///  ::= .thumb
bool ARMAsmParser::parseDirectiveThumb(SMLoc L) {
  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(L, "unexpected token in directive");
  Parser.Lex();

  // TODO: set thumb mode
  // TODO: tell the MC streamer the mode
  // getParser().getStreamer().Emit???();
  return false;
}

/// parseDirectiveThumbFunc
///  ::= .thumbfunc symbol_name
bool ARMAsmParser::parseDirectiveThumbFunc(SMLoc L) {
  const MCAsmInfo &MAI = getParser().getStreamer().getContext().getAsmInfo();
  bool isMachO = MAI.hasSubsectionsViaSymbols();
  StringRef Name;

  // Darwin asm has function name after .thumb_func direction
  // ELF doesn't
  if (isMachO) {
    const AsmToken &Tok = Parser.getTok();
    if (Tok.isNot(AsmToken::Identifier) && Tok.isNot(AsmToken::String))
      return Error(L, "unexpected token in .thumb_func directive");
    Name = Tok.getString();
    Parser.Lex(); // Consume the identifier token.
  }

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(L, "unexpected token in directive");
  Parser.Lex();

  // FIXME: assuming function name will be the line following .thumb_func
  if (!isMachO) {
    Name = Parser.getTok().getString();
  }

  // Mark symbol as a thumb symbol.
  MCSymbol *Func = getParser().getContext().GetOrCreateSymbol(Name);
  getParser().getStreamer().EmitThumbFunc(Func);
  return false;
}

/// parseDirectiveSyntax
///  ::= .syntax unified | divided
bool ARMAsmParser::parseDirectiveSyntax(SMLoc L) {
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return Error(L, "unexpected token in .syntax directive");
  StringRef Mode = Tok.getString();
  if (Mode == "unified" || Mode == "UNIFIED")
    Parser.Lex();
  else if (Mode == "divided" || Mode == "DIVIDED")
    return Error(L, "'.syntax divided' arm asssembly not supported");
  else
    return Error(L, "unrecognized syntax mode in .syntax directive");

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(Parser.getTok().getLoc(), "unexpected token in directive");
  Parser.Lex();

  // TODO tell the MC streamer the mode
  // getParser().getStreamer().Emit???();
  return false;
}

/// parseDirectiveCode
///  ::= .code 16 | 32
bool ARMAsmParser::parseDirectiveCode(SMLoc L) {
  const AsmToken &Tok = Parser.getTok();
  if (Tok.isNot(AsmToken::Integer))
    return Error(L, "unexpected token in .code directive");
  int64_t Val = Parser.getTok().getIntVal();
  if (Val == 16)
    Parser.Lex();
  else if (Val == 32)
    Parser.Lex();
  else
    return Error(L, "invalid operand to .code directive");

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(Parser.getTok().getLoc(), "unexpected token in directive");
  Parser.Lex();

  if (Val == 16) {
    if (!isThumb())
      SwitchMode();
    getParser().getStreamer().EmitAssemblerFlag(MCAF_Code16);
  } else {
    if (isThumb())
      SwitchMode();
    getParser().getStreamer().EmitAssemblerFlag(MCAF_Code32);
  }

  return false;
}

extern "C" void LLVMInitializeARMAsmLexer();

/// Force static initialization.
extern "C" void LLVMInitializeARMAsmParser() {
  RegisterMCAsmParser<ARMAsmParser> X(TheARMTarget);
  RegisterMCAsmParser<ARMAsmParser> Y(TheThumbTarget);
  LLVMInitializeARMAsmLexer();
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "ARMGenAsmMatcher.inc"
