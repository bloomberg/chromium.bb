//===-- MBlazeAsmBackend.cpp - MBlaze Assembler Backend -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MBlazeMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCELFSymbolFlags.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCValue.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

static unsigned getFixupKindSize(unsigned Kind) {
  switch (Kind) {
  default: assert(0 && "invalid fixup kind!");
  case FK_Data_1: return 1;
  case FK_PCRel_2:
  case FK_Data_2: return 2;
  case FK_PCRel_4:
  case FK_Data_4: return 4;
  case FK_Data_8: return 8;
  }
}


namespace {
class MBlazeELFObjectWriter : public MCELFObjectTargetWriter {
public:
  MBlazeELFObjectWriter(Triple::OSType OSType)
    : MCELFObjectTargetWriter(/*is64Bit*/ false, OSType, ELF::EM_MBLAZE,
                              /*HasRelocationAddend*/ true) {}
};

class MBlazeAsmBackend : public MCAsmBackend {
public:
  MBlazeAsmBackend(const Target &T)
    : MCAsmBackend() {
  }

  unsigned getNumFixupKinds() const {
    return 2;
  }

  bool MayNeedRelaxation(const MCInst &Inst) const;

  void RelaxInstruction(const MCInst &Inst, MCInst &Res) const;

  bool WriteNopData(uint64_t Count, MCObjectWriter *OW) const;

  unsigned getPointerSize() const {
    return 4;
  }
};

static unsigned getRelaxedOpcode(unsigned Op) {
    switch (Op) {
    default:            return Op;
    case MBlaze::ADDIK: return MBlaze::ADDIK32;
    case MBlaze::ORI:   return MBlaze::ORI32;
    case MBlaze::BRLID: return MBlaze::BRLID32;
    }
}

bool MBlazeAsmBackend::MayNeedRelaxation(const MCInst &Inst) const {
  if (getRelaxedOpcode(Inst.getOpcode()) == Inst.getOpcode())
    return false;

  bool hasExprOrImm = false;
  for (unsigned i = 0; i < Inst.getNumOperands(); ++i)
    hasExprOrImm |= Inst.getOperand(i).isExpr();

  return hasExprOrImm;
}

void MBlazeAsmBackend::RelaxInstruction(const MCInst &Inst, MCInst &Res) const {
  Res = Inst;
  Res.setOpcode(getRelaxedOpcode(Inst.getOpcode()));
}

bool MBlazeAsmBackend::WriteNopData(uint64_t Count, MCObjectWriter *OW) const {
  if ((Count % 4) != 0)
    return false;

  for (uint64_t i = 0; i < Count; i += 4)
      OW->Write32(0x00000000);

  return true;
}
} // end anonymous namespace

namespace {
class ELFMBlazeAsmBackend : public MBlazeAsmBackend {
public:
  Triple::OSType OSType;
  ELFMBlazeAsmBackend(const Target &T, Triple::OSType _OSType)
    : MBlazeAsmBackend(T), OSType(_OSType) { }

  void ApplyFixup(const MCFixup &Fixup, char *Data, unsigned DataSize,
                  uint64_t Value) const;

  MCObjectWriter *createObjectWriter(raw_ostream &OS) const {
    return createELFObjectWriter(new MBlazeELFObjectWriter(OSType), OS,
                                 /*IsLittleEndian*/ false);
  }
};

void ELFMBlazeAsmBackend::ApplyFixup(const MCFixup &Fixup, char *Data,
                                     unsigned DataSize, uint64_t Value) const {
  unsigned Size = getFixupKindSize(Fixup.getKind());

  assert(Fixup.getOffset() + Size <= DataSize &&
         "Invalid fixup offset!");

  char *data = Data + Fixup.getOffset();
  switch (Size) {
  default: llvm_unreachable("Cannot fixup unknown value.");
  case 1:  llvm_unreachable("Cannot fixup 1 byte value.");
  case 8:  llvm_unreachable("Cannot fixup 8 byte value.");

  case 4:
    *(data+7) = uint8_t(Value);
    *(data+6) = uint8_t(Value >> 8);
    *(data+3) = uint8_t(Value >> 16);
    *(data+2) = uint8_t(Value >> 24);
    break;

  case 2:
    *(data+3) = uint8_t(Value >> 0);
    *(data+2) = uint8_t(Value >> 8);
  }
}
} // end anonymous namespace

MCAsmBackend *llvm::createMBlazeAsmBackend(const Target &T, StringRef TT) {
  Triple TheTriple(TT);

  if (TheTriple.isOSDarwin())
    assert(0 && "Mac not supported on MBlaze");

  if (TheTriple.isOSWindows())
    assert(0 && "Windows not supported on MBlaze");

  return new ELFMBlazeAsmBackend(T, TheTriple.getOS());
}
