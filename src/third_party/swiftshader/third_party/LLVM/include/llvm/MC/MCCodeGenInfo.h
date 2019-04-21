//===-- llvm/MC/MCCodeGenInfo.h - Target CodeGen Info -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file tracks information about the target which can affect codegen,
// asm parsing, and asm printing. For example, relocation model.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCCODEGENINFO_H
#define LLVM_MC_MCCODEGENINFO_H

#include "llvm/Support/CodeGen.h"

namespace llvm {

  class MCCodeGenInfo {
    /// RelocationModel - Relocation model: statcic, pic, etc.
    ///
    Reloc::Model RelocationModel;

    /// CMModel - Code model.
    ///
    CodeModel::Model CMModel;

  public:
    void InitMCCodeGenInfo(Reloc::Model RM = Reloc::Default,
                           CodeModel::Model CM = CodeModel::Default);

    Reloc::Model getRelocationModel() const { return RelocationModel; }

    CodeModel::Model getCodeModel() const { return CMModel; }
  };
} // namespace llvm

#endif
