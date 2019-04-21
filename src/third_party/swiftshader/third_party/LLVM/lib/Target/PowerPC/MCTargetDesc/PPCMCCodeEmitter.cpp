//===-- PPCMCCodeEmitter.cpp - Convert PPC code to machine code -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the PPCMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "mccodeemitter"
#include "MCTargetDesc/PPCBaseInfo.h"
#include "MCTargetDesc/PPCFixupKinds.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");

namespace {
class PPCMCCodeEmitter : public MCCodeEmitter {
  PPCMCCodeEmitter(const PPCMCCodeEmitter &); // DO NOT IMPLEMENT
  void operator=(const PPCMCCodeEmitter &);   // DO NOT IMPLEMENT
  
public:
  PPCMCCodeEmitter(const MCInstrInfo &mcii, const MCSubtargetInfo &sti,
                   MCContext &ctx) {
  }
  
  ~PPCMCCodeEmitter() {}

  unsigned getDirectBrEncoding(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups) const;
  unsigned getCondBrEncoding(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups) const;
  unsigned getHA16Encoding(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups) const;
  unsigned getLO16Encoding(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups) const;
  unsigned getMemRIEncoding(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups) const;
  unsigned getMemRIXEncoding(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups) const;
  unsigned get_crbitm_encoding(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups) const;

  /// getMachineOpValue - Return binary encoding of operand. If the machine
  /// operand requires relocation, record the relocation and return zero.
  unsigned getMachineOpValue(const MCInst &MI,const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups) const;
  
  // getBinaryCodeForInstr - TableGen'erated function for getting the
  // binary encoding for an instruction.
  unsigned getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups) const;
  void EncodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups) const {
    unsigned Bits = getBinaryCodeForInstr(MI, Fixups);
    
    // Output the constant in big endian byte order.
    for (unsigned i = 0; i != 4; ++i) {
      OS << (char)(Bits >> 24);
      Bits <<= 8;
    }
    
    ++MCNumEmitted;  // Keep track of the # of mi's emitted.
  }
  
};
  
} // end anonymous namespace
  
MCCodeEmitter *llvm::createPPCMCCodeEmitter(const MCInstrInfo &MCII,
                                            const MCSubtargetInfo &STI,
                                            MCContext &Ctx) {
  return new PPCMCCodeEmitter(MCII, STI, Ctx);
}

unsigned PPCMCCodeEmitter::
getDirectBrEncoding(const MCInst &MI, unsigned OpNo,
                    SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg() || MO.isImm()) return getMachineOpValue(MI, MO, Fixups);
  
  // Add a fixup for the branch target.
  Fixups.push_back(MCFixup::Create(0, MO.getExpr(),
                                   (MCFixupKind)PPC::fixup_ppc_br24));
  return 0;
}

unsigned PPCMCCodeEmitter::getCondBrEncoding(const MCInst &MI, unsigned OpNo,
                                     SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg() || MO.isImm()) return getMachineOpValue(MI, MO, Fixups);

  // Add a fixup for the branch target.
  Fixups.push_back(MCFixup::Create(0, MO.getExpr(),
                                   (MCFixupKind)PPC::fixup_ppc_brcond14));
  return 0;
}

unsigned PPCMCCodeEmitter::getHA16Encoding(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg() || MO.isImm()) return getMachineOpValue(MI, MO, Fixups);
  
  // Add a fixup for the branch target.
  Fixups.push_back(MCFixup::Create(0, MO.getExpr(),
                                   (MCFixupKind)PPC::fixup_ppc_ha16));
  return 0;
}

unsigned PPCMCCodeEmitter::getLO16Encoding(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg() || MO.isImm()) return getMachineOpValue(MI, MO, Fixups);
  
  // Add a fixup for the branch target.
  Fixups.push_back(MCFixup::Create(0, MO.getExpr(),
                                   (MCFixupKind)PPC::fixup_ppc_lo16));
  return 0;
}

unsigned PPCMCCodeEmitter::getMemRIEncoding(const MCInst &MI, unsigned OpNo,
                                            SmallVectorImpl<MCFixup> &Fixups) const {
  // Encode (imm, reg) as a memri, which has the low 16-bits as the
  // displacement and the next 5 bits as the register #.
  assert(MI.getOperand(OpNo+1).isReg());
  unsigned RegBits = getMachineOpValue(MI, MI.getOperand(OpNo+1), Fixups) << 16;
  
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return (getMachineOpValue(MI, MO, Fixups) & 0xFFFF) | RegBits;
  
  // Add a fixup for the displacement field.
  Fixups.push_back(MCFixup::Create(0, MO.getExpr(),
                                   (MCFixupKind)PPC::fixup_ppc_lo16));
  return RegBits;
}


unsigned PPCMCCodeEmitter::getMemRIXEncoding(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups) const {
  // Encode (imm, reg) as a memrix, which has the low 14-bits as the
  // displacement and the next 5 bits as the register #.
  assert(MI.getOperand(OpNo+1).isReg());
  unsigned RegBits = getMachineOpValue(MI, MI.getOperand(OpNo+1), Fixups) << 14;
  
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm())
    return (getMachineOpValue(MI, MO, Fixups) & 0x3FFF) | RegBits;
  
  // Add a fixup for the branch target.
  Fixups.push_back(MCFixup::Create(0, MO.getExpr(),
                                   (MCFixupKind)PPC::fixup_ppc_lo14));
  return RegBits;
}


unsigned PPCMCCodeEmitter::
get_crbitm_encoding(const MCInst &MI, unsigned OpNo,
                    SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  assert((MI.getOpcode() == PPC::MTCRF || MI.getOpcode() == PPC::MFOCRF) &&
         (MO.getReg() >= PPC::CR0 && MO.getReg() <= PPC::CR7));
  return 0x80 >> getPPCRegisterNumbering(MO.getReg());
}


unsigned PPCMCCodeEmitter::
getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                  SmallVectorImpl<MCFixup> &Fixups) const {
  if (MO.isReg()) {
    // MTCRF/MFOCRF should go through get_crbitm_encoding for the CR operand.
    // The GPR operand should come through here though.
    assert((MI.getOpcode() != PPC::MTCRF && MI.getOpcode() != PPC::MFOCRF) ||
           MO.getReg() < PPC::CR0 || MO.getReg() > PPC::CR7);
    return getPPCRegisterNumbering(MO.getReg());
  }
  
  assert(MO.isImm() &&
         "Relocation required in an instruction that we cannot encode!");
  return MO.getImm();
}


#include "PPCGenMCCodeEmitter.inc"
