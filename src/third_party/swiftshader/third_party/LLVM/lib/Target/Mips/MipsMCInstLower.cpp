//===-- MipsMCInstLower.cpp - Convert Mips MachineInstr to MCInst ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains code to lower Mips MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "MipsMCInstLower.h"
#include "MipsAsmPrinter.h"
#include "MipsInstrInfo.h"
#include "MipsMCSymbolRefExpr.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Target/Mangler.h"
using namespace llvm;

MipsMCInstLower::MipsMCInstLower(Mangler *mang, const MachineFunction &mf,
                                 MipsAsmPrinter &asmprinter)
  : Ctx(mf.getContext()), Mang(mang), AsmPrinter(asmprinter) {}

MCOperand MipsMCInstLower::LowerSymbolOperand(const MachineOperand &MO,
                                              MachineOperandType MOTy,
                                              unsigned Offset) const {
  MipsMCSymbolRefExpr::VariantKind Kind;
  const MCSymbol *Symbol;

  switch(MO.getTargetFlags()) {
  default:                  assert(0 && "Invalid target flag!");
  case MipsII::MO_NO_FLAG:  Kind = MipsMCSymbolRefExpr::VK_Mips_None; break;
  case MipsII::MO_GPREL:    Kind = MipsMCSymbolRefExpr::VK_Mips_GPREL; break;
  case MipsII::MO_GOT_CALL: Kind = MipsMCSymbolRefExpr::VK_Mips_GOT_CALL; break;
  case MipsII::MO_GOT:      Kind = MipsMCSymbolRefExpr::VK_Mips_GOT; break;
  case MipsII::MO_ABS_HI:   Kind = MipsMCSymbolRefExpr::VK_Mips_ABS_HI; break;
  case MipsII::MO_ABS_LO:   Kind = MipsMCSymbolRefExpr::VK_Mips_ABS_LO; break;
  case MipsII::MO_TLSGD:    Kind = MipsMCSymbolRefExpr::VK_Mips_TLSGD; break;
  case MipsII::MO_GOTTPREL: Kind = MipsMCSymbolRefExpr::VK_Mips_GOTTPREL; break;
  case MipsII::MO_TPREL_HI: Kind = MipsMCSymbolRefExpr::VK_Mips_TPREL_HI; break;
  case MipsII::MO_TPREL_LO: Kind = MipsMCSymbolRefExpr::VK_Mips_TPREL_LO; break;
  case MipsII::MO_GPOFF_HI: Kind = MipsMCSymbolRefExpr::VK_Mips_GPOFF_HI; break;
  case MipsII::MO_GPOFF_LO: Kind = MipsMCSymbolRefExpr::VK_Mips_GPOFF_LO; break;
  case MipsII::MO_GOT_DISP: Kind = MipsMCSymbolRefExpr::VK_Mips_GOT_DISP; break;
  case MipsII::MO_GOT_PAGE: Kind = MipsMCSymbolRefExpr::VK_Mips_GOT_PAGE; break;
  case MipsII::MO_GOT_OFST: Kind = MipsMCSymbolRefExpr::VK_Mips_GOT_OFST; break;
  }

  switch (MOTy) {
    case MachineOperand::MO_MachineBasicBlock:
      Symbol = MO.getMBB()->getSymbol();
      break;

    case MachineOperand::MO_GlobalAddress:
      Symbol = Mang->getSymbol(MO.getGlobal());
      break;

    case MachineOperand::MO_BlockAddress:
      Symbol = AsmPrinter.GetBlockAddressSymbol(MO.getBlockAddress());
      break;

    case MachineOperand::MO_ExternalSymbol:
      Symbol = AsmPrinter.GetExternalSymbolSymbol(MO.getSymbolName());
      break;

    case MachineOperand::MO_JumpTableIndex:
      Symbol = AsmPrinter.GetJTISymbol(MO.getIndex());
      break;

    case MachineOperand::MO_ConstantPoolIndex:
      Symbol = AsmPrinter.GetCPISymbol(MO.getIndex());
      if (MO.getOffset())
        Offset += MO.getOffset();  
      break;

    default:
      llvm_unreachable("<unknown operand type>");
  }
  
  return MCOperand::CreateExpr(MipsMCSymbolRefExpr::Create(Kind, Symbol, Offset,
                                                           Ctx));
}

MCOperand MipsMCInstLower::LowerOperand(const MachineOperand& MO) const {
  MachineOperandType MOTy = MO.getType();
  
  switch (MOTy) {
  default:
    assert(0 && "unknown operand type");
    break;
  case MachineOperand::MO_Register:
    // Ignore all implicit register operands.
    if (MO.isImplicit()) break;
    return MCOperand::CreateReg(MO.getReg());
  case MachineOperand::MO_Immediate:
    return MCOperand::CreateImm(MO.getImm());
  case MachineOperand::MO_MachineBasicBlock:
  case MachineOperand::MO_GlobalAddress:
  case MachineOperand::MO_ExternalSymbol:
  case MachineOperand::MO_JumpTableIndex:
  case MachineOperand::MO_ConstantPoolIndex:
  case MachineOperand::MO_BlockAddress:
    return LowerSymbolOperand(MO, MOTy, 0);
 }

  return MCOperand();
}

void MipsMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());
  
  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    MCOperand MCOp = LowerOperand(MO);

    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}
