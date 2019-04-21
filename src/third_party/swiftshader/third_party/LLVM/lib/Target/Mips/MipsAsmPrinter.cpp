//===-- MipsAsmPrinter.cpp - Mips LLVM assembly writer --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format MIPS assembly language.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "mips-asm-printer"
#include "MipsAsmPrinter.h"
#include "Mips.h"
#include "MipsInstrInfo.h"
#include "MipsMachineFunction.h"
#include "MipsMCInstLower.h"
#include "MipsMCSymbolRefExpr.h"
#include "InstPrinter/MipsInstPrinter.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instructions.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/DebugInfo.h"

using namespace llvm;

static bool isUnalignedLoadStore(unsigned Opc) {
  return Opc == Mips::ULW    || Opc == Mips::ULH    || Opc == Mips::ULHu ||
         Opc == Mips::USW    || Opc == Mips::USH    ||
         Opc == Mips::ULW_P8 || Opc == Mips::ULH_P8 || Opc == Mips::ULHu_P8 ||
         Opc == Mips::USW_P8 || Opc == Mips::USH_P8;
}

void MipsAsmPrinter::EmitInstruction(const MachineInstr *MI) {
  SmallString<128> Str;
  raw_svector_ostream OS(Str);

  if (MI->isDebugValue()) {
    PrintDebugValueComment(MI, OS);
    return;
  }

  MipsMCInstLower MCInstLowering(Mang, *MF, *this);
  unsigned Opc = MI->getOpcode();
  MCInst TmpInst0;
  MCInstLowering.Lower(MI, TmpInst0);
  
  // Enclose unaligned load or store with .macro & .nomacro directives.
  if (isUnalignedLoadStore(Opc)) {
    MCInst Directive;
    Directive.setOpcode(Mips::MACRO);
    OutStreamer.EmitInstruction(Directive);
    OutStreamer.EmitInstruction(TmpInst0);
    Directive.setOpcode(Mips::NOMACRO);
    OutStreamer.EmitInstruction(Directive);
    return;
  }

  OutStreamer.EmitInstruction(TmpInst0);
}

//===----------------------------------------------------------------------===//
//
//  Mips Asm Directives
//
//  -- Frame directive "frame Stackpointer, Stacksize, RARegister"
//  Describe the stack frame.
//
//  -- Mask directives "(f)mask  bitmask, offset"
//  Tells the assembler which registers are saved and where.
//  bitmask - contain a little endian bitset indicating which registers are
//            saved on function prologue (e.g. with a 0x80000000 mask, the
//            assembler knows the register 31 (RA) is saved at prologue.
//  offset  - the position before stack pointer subtraction indicating where
//            the first saved register on prologue is located. (e.g. with a
//
//  Consider the following function prologue:
//
//    .frame  $fp,48,$ra
//    .mask   0xc0000000,-8
//       addiu $sp, $sp, -48
//       sw $ra, 40($sp)
//       sw $fp, 36($sp)
//
//    With a 0xc0000000 mask, the assembler knows the register 31 (RA) and
//    30 (FP) are saved at prologue. As the save order on prologue is from
//    left to right, RA is saved first. A -8 offset means that after the
//    stack pointer subtration, the first register in the mask (RA) will be
//    saved at address 48-8=40.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Mask directives
//===----------------------------------------------------------------------===//

// Create a bitmask with all callee saved registers for CPU or Floating Point
// registers. For CPU registers consider RA, GP and FP for saving if necessary.
void MipsAsmPrinter::printSavedRegsBitmask(raw_ostream &O) {
  // CPU and FPU Saved Registers Bitmasks
  unsigned CPUBitmask = 0, FPUBitmask = 0;
  int CPUTopSavedRegOff, FPUTopSavedRegOff;

  // Set the CPU and FPU Bitmasks
  const MachineFrameInfo *MFI = MF->getFrameInfo();
  const std::vector<CalleeSavedInfo> &CSI = MFI->getCalleeSavedInfo();
  // size of stack area to which FP callee-saved regs are saved.
  unsigned CPURegSize = Mips::CPURegsRegisterClass->getSize();
  unsigned FGR32RegSize = Mips::FGR32RegisterClass->getSize();
  unsigned AFGR64RegSize = Mips::AFGR64RegisterClass->getSize();
  bool HasAFGR64Reg = false;
  unsigned CSFPRegsSize = 0;
  unsigned i, e = CSI.size();

  // Set FPU Bitmask.
  for (i = 0; i != e; ++i) {
    unsigned Reg = CSI[i].getReg();
    if (Mips::CPURegsRegisterClass->contains(Reg))
      break;

    unsigned RegNum = MipsRegisterInfo::getRegisterNumbering(Reg);
    if (Mips::AFGR64RegisterClass->contains(Reg)) {
      FPUBitmask |= (3 << RegNum);
      CSFPRegsSize += AFGR64RegSize;
      HasAFGR64Reg = true;
      continue;
    }

    FPUBitmask |= (1 << RegNum);
    CSFPRegsSize += FGR32RegSize;
  }

  // Set CPU Bitmask.
  for (; i != e; ++i) {
    unsigned Reg = CSI[i].getReg();
    unsigned RegNum = MipsRegisterInfo::getRegisterNumbering(Reg);
    CPUBitmask |= (1 << RegNum);
  }

  // FP Regs are saved right below where the virtual frame pointer points to.
  FPUTopSavedRegOff = FPUBitmask ?
    (HasAFGR64Reg ? -AFGR64RegSize : -FGR32RegSize) : 0;

  // CPU Regs are saved below FP Regs.
  CPUTopSavedRegOff = CPUBitmask ? -CSFPRegsSize - CPURegSize : 0;

  // Print CPUBitmask
  O << "\t.mask \t"; printHex32(CPUBitmask, O);
  O << ',' << CPUTopSavedRegOff << '\n';

  // Print FPUBitmask
  O << "\t.fmask\t"; printHex32(FPUBitmask, O);
  O << "," << FPUTopSavedRegOff << '\n';
}

// Print a 32 bit hex number with all numbers.
void MipsAsmPrinter::printHex32(unsigned Value, raw_ostream &O) {
  O << "0x";
  for (int i = 7; i >= 0; i--)
    O << utohexstr((Value & (0xF << (i*4))) >> (i*4));
}

//===----------------------------------------------------------------------===//
// Frame and Set directives
//===----------------------------------------------------------------------===//

/// Frame Directive
void MipsAsmPrinter::emitFrameDirective() {
  const TargetRegisterInfo &RI = *TM.getRegisterInfo();

  unsigned stackReg  = RI.getFrameRegister(*MF);
  unsigned returnReg = RI.getRARegister();
  unsigned stackSize = MF->getFrameInfo()->getStackSize();

  OutStreamer.EmitRawText("\t.frame\t$" +
           Twine(LowercaseString(MipsInstPrinter::getRegisterName(stackReg))) +
           "," + Twine(stackSize) + ",$" +
           Twine(LowercaseString(MipsInstPrinter::getRegisterName(returnReg))));
}

/// Emit Set directives.
const char *MipsAsmPrinter::getCurrentABIString() const {
  switch (Subtarget->getTargetABI()) {
  case MipsSubtarget::O32:  return "abi32";
  case MipsSubtarget::N32:  return "abiN32";
  case MipsSubtarget::N64:  return "abi64";
  case MipsSubtarget::EABI: return "eabi32"; // TODO: handle eabi64
  default: break;
  }

  llvm_unreachable("Unknown Mips ABI");
  return NULL;
}

void MipsAsmPrinter::EmitFunctionEntryLabel() {
  OutStreamer.EmitRawText("\t.ent\t" + Twine(CurrentFnSym->getName()));
  OutStreamer.EmitLabel(CurrentFnSym);
}

/// EmitFunctionBodyStart - Targets can override this to emit stuff before
/// the first basic block in the function.
void MipsAsmPrinter::EmitFunctionBodyStart() {
  emitFrameDirective();

  SmallString<128> Str;
  raw_svector_ostream OS(Str);
  printSavedRegsBitmask(OS);
  OutStreamer.EmitRawText(OS.str());
}

/// EmitFunctionBodyEnd - Targets can override this to emit stuff after
/// the last basic block in the function.
void MipsAsmPrinter::EmitFunctionBodyEnd() {
  // There are instruction for this macros, but they must
  // always be at the function end, and we can't emit and
  // break with BB logic.
  OutStreamer.EmitRawText(StringRef("\t.set\tmacro"));
  OutStreamer.EmitRawText(StringRef("\t.set\treorder"));
  OutStreamer.EmitRawText("\t.end\t" + Twine(CurrentFnSym->getName()));
}


/// isBlockOnlyReachableByFallthough - Return true if the basic block has
/// exactly one predecessor and the control transfer mechanism between
/// the predecessor and this block is a fall-through.
bool MipsAsmPrinter::isBlockOnlyReachableByFallthrough(const MachineBasicBlock*
                                                       MBB) const {
  // The predecessor has to be immediately before this block.
  const MachineBasicBlock *Pred = *MBB->pred_begin();

  // If the predecessor is a switch statement, assume a jump table
  // implementation, so it is not a fall through.
  if (const BasicBlock *bb = Pred->getBasicBlock())
    if (isa<SwitchInst>(bb->getTerminator()))
      return false;

  // If this is a landing pad, it isn't a fall through.  If it has no preds,
  // then nothing falls through to it.
  if (MBB->isLandingPad() || MBB->pred_empty())
    return false;

  // If there isn't exactly one predecessor, it can't be a fall through.
  MachineBasicBlock::const_pred_iterator PI = MBB->pred_begin(), PI2 = PI;
  ++PI2;
 
  if (PI2 != MBB->pred_end())
    return false;  

  // The predecessor has to be immediately before this block.
  if (!Pred->isLayoutSuccessor(MBB))
    return false;
   
  // If the block is completely empty, then it definitely does fall through.
  if (Pred->empty())
    return true;
  
  // Otherwise, check the last instruction.
  // Check if the last terminator is an unconditional branch.
  MachineBasicBlock::const_iterator I = Pred->end();
  while (I != Pred->begin() && !(--I)->getDesc().isTerminator()) ;

  return !I->getDesc().isBarrier();
}

// Print out an operand for an inline asm expression.
bool MipsAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                     unsigned AsmVariant,const char *ExtraCode,
                                     raw_ostream &O) {
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0])
    return true; // Unknown modifier.

  printOperand(MI, OpNo, O);
  return false;
}

bool MipsAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                           unsigned OpNum, unsigned AsmVariant,
                                           const char *ExtraCode,
                                           raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
     return true; // Unknown modifier.
   
  const MachineOperand &MO = MI->getOperand(OpNum);
  assert(MO.isReg() && "unexpected inline asm memory operand");
  O << "0($" << MipsInstPrinter::getRegisterName(MO.getReg()) << ")";
  return false;
}

void MipsAsmPrinter::printOperand(const MachineInstr *MI, int opNum,
                                  raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(opNum);
  bool closeP = false;

  if (MO.getTargetFlags())
    closeP = true;

  switch(MO.getTargetFlags()) {
  case MipsII::MO_GPREL:    O << "%gp_rel("; break;
  case MipsII::MO_GOT_CALL: O << "%call16("; break;
  case MipsII::MO_GOT:      O << "%got(";    break;
  case MipsII::MO_ABS_HI:   O << "%hi(";     break;
  case MipsII::MO_ABS_LO:   O << "%lo(";     break;
  case MipsII::MO_TLSGD:    O << "%tlsgd(";  break;
  case MipsII::MO_GOTTPREL: O << "%gottprel("; break;
  case MipsII::MO_TPREL_HI: O << "%tprel_hi("; break;
  case MipsII::MO_TPREL_LO: O << "%tprel_lo("; break;
  case MipsII::MO_GPOFF_HI: O << "%hi(%neg(%gp_rel("; break;
  case MipsII::MO_GPOFF_LO: O << "%lo(%neg(%gp_rel("; break;
  case MipsII::MO_GOT_DISP: O << "%got_disp("; break;
  case MipsII::MO_GOT_PAGE: O << "%got_page("; break;
  case MipsII::MO_GOT_OFST: O << "%got_ofst("; break;
  }

  switch (MO.getType()) {
    case MachineOperand::MO_Register:
      O << '$'
        << LowercaseString(MipsInstPrinter::getRegisterName(MO.getReg()));
      break;

    case MachineOperand::MO_Immediate:
      O << MO.getImm();
      break;

    case MachineOperand::MO_MachineBasicBlock:
      O << *MO.getMBB()->getSymbol();
      return;

    case MachineOperand::MO_GlobalAddress:
      O << *Mang->getSymbol(MO.getGlobal());
      break;

    case MachineOperand::MO_BlockAddress: {
      MCSymbol* BA = GetBlockAddressSymbol(MO.getBlockAddress());
      O << BA->getName();
      break;
    }

    case MachineOperand::MO_ExternalSymbol:
      O << *GetExternalSymbolSymbol(MO.getSymbolName());
      break;

    case MachineOperand::MO_JumpTableIndex:
      O << MAI->getPrivateGlobalPrefix() << "JTI" << getFunctionNumber()
        << '_' << MO.getIndex();
      break;

    case MachineOperand::MO_ConstantPoolIndex:
      O << MAI->getPrivateGlobalPrefix() << "CPI"
        << getFunctionNumber() << "_" << MO.getIndex();
      if (MO.getOffset())
        O << "+" << MO.getOffset();
      break;

    default:
      llvm_unreachable("<unknown operand type>");
  }

  if (closeP) O << ")";
}

void MipsAsmPrinter::printUnsignedImm(const MachineInstr *MI, int opNum,
                                      raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(opNum);
  if (MO.isImm())
    O << (unsigned short int)MO.getImm();
  else
    printOperand(MI, opNum, O);
}

void MipsAsmPrinter::
printMemOperand(const MachineInstr *MI, int opNum, raw_ostream &O) {
  // Load/Store memory operands -- imm($reg)
  // If PIC target the target is loaded as the
  // pattern lw $25,%call16($28)
  printOperand(MI, opNum+1, O);
  O << "(";
  printOperand(MI, opNum, O);
  O << ")";
}

void MipsAsmPrinter::
printMemOperandEA(const MachineInstr *MI, int opNum, raw_ostream &O) {
  // when using stack locations for not load/store instructions
  // print the same way as all normal 3 operand instructions.
  printOperand(MI, opNum, O);
  O << ", ";
  printOperand(MI, opNum+1, O);
  return;
}

void MipsAsmPrinter::
printFCCOperand(const MachineInstr *MI, int opNum, raw_ostream &O,
                const char *Modifier) {
  const MachineOperand& MO = MI->getOperand(opNum);
  O << Mips::MipsFCCToString((Mips::CondCode)MO.getImm());
}

void MipsAsmPrinter::EmitStartOfAsmFile(Module &M) {
  // FIXME: Use SwitchSection.

  // Tell the assembler which ABI we are using
  OutStreamer.EmitRawText("\t.section .mdebug." + Twine(getCurrentABIString()));

  // TODO: handle O64 ABI
  if (Subtarget->isABI_EABI()) {
    if (Subtarget->isGP32bit())
      OutStreamer.EmitRawText(StringRef("\t.section .gcc_compiled_long32"));
    else
      OutStreamer.EmitRawText(StringRef("\t.section .gcc_compiled_long64"));
  }

  // return to previous section
  OutStreamer.EmitRawText(StringRef("\t.previous"));
}

MachineLocation
MipsAsmPrinter::getDebugValueLocation(const MachineInstr *MI) const {
  // Handles frame addresses emitted in MipsInstrInfo::emitFrameIndexDebugValue.
  assert(MI->getNumOperands() == 4 && "Invalid no. of machine operands!");
  assert(MI->getOperand(0).isReg() && MI->getOperand(1).isImm() &&
         "Unexpected MachineOperand types");
  return MachineLocation(MI->getOperand(0).getReg(),
                         MI->getOperand(1).getImm());
}

void MipsAsmPrinter::PrintDebugValueComment(const MachineInstr *MI,
                                           raw_ostream &OS) {
  // TODO: implement
}

// Force static initialization.
extern "C" void LLVMInitializeMipsAsmPrinter() {
  RegisterAsmPrinter<MipsAsmPrinter> X(TheMipsTarget);
  RegisterAsmPrinter<MipsAsmPrinter> Y(TheMipselTarget);
  RegisterAsmPrinter<MipsAsmPrinter> A(TheMips64Target);
  RegisterAsmPrinter<MipsAsmPrinter> B(TheMips64elTarget);
}
