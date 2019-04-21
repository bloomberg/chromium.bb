//===-- SystemZMCAsmInfo.cpp - SystemZ asm properties ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the SystemZMCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "SystemZMCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Support/ELF.h"
using namespace llvm;

SystemZMCAsmInfo::SystemZMCAsmInfo(const Target &T, StringRef TT) {
  IsLittleEndian = false;
  PointerSize = 8;
  PrivateGlobalPrefix = ".L";
  WeakRefDirective = "\t.weak\t";
  PCSymbol = ".";
}

const MCSection *SystemZMCAsmInfo::
getNonexecutableStackSection(MCContext &Ctx) const{
  return Ctx.getELFSection(".note.GNU-stack", ELF::SHT_PROGBITS,
                           0, SectionKind::getMetadata());
}
