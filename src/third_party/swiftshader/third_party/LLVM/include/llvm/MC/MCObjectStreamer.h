//===- MCObjectStreamer.h - MCStreamer Object File Interface ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCOBJECTSTREAMER_H
#define LLVM_MC_MCOBJECTSTREAMER_H

#include "llvm/MC/MCStreamer.h"

namespace llvm {
class MCAssembler;
class MCCodeEmitter;
class MCSectionData;
class MCExpr;
class MCFragment;
class MCDataFragment;
class MCAsmBackend;
class raw_ostream;

/// \brief Streaming object file generation interface.
///
/// This class provides an implementation of the MCStreamer interface which is
/// suitable for use with the assembler backend. Specific object file formats
/// are expected to subclass this interface to implement directives specific
/// to that file format or custom semantics expected by the object writer
/// implementation.
class MCObjectStreamer : public MCStreamer {
  MCAssembler *Assembler;
  MCSectionData *CurSectionData;

  virtual void EmitInstToData(const MCInst &Inst) = 0;

protected:
  MCObjectStreamer(MCContext &Context, MCAsmBackend &TAB,
                   raw_ostream &_OS, MCCodeEmitter *_Emitter);
  MCObjectStreamer(MCContext &Context, MCAsmBackend &TAB,
                   raw_ostream &_OS, MCCodeEmitter *_Emitter,
                   MCAssembler *_Assembler);
  ~MCObjectStreamer();

  MCSectionData *getCurrentSectionData() const {
    return CurSectionData;
  }

  MCFragment *getCurrentFragment() const;

  /// Get a data fragment to write into, creating a new one if the current
  /// fragment is not a data fragment.
  MCDataFragment *getOrCreateDataFragment() const;

  const MCExpr *AddValueSymbols(const MCExpr *Value);

public:
  MCAssembler &getAssembler() { return *Assembler; }

  /// @name MCStreamer Interface
  /// @{

  virtual void EmitLabel(MCSymbol *Symbol);
  virtual void EmitValueImpl(const MCExpr *Value, unsigned Size,
                             unsigned AddrSpace);
  virtual void EmitULEB128Value(const MCExpr *Value);
  virtual void EmitSLEB128Value(const MCExpr *Value);
  virtual void EmitWeakReference(MCSymbol *Alias, const MCSymbol *Symbol);
  virtual void ChangeSection(const MCSection *Section);
  virtual void EmitInstruction(const MCInst &Inst);
  virtual void EmitInstToFragment(const MCInst &Inst);
  virtual void EmitValueToOffset(const MCExpr *Offset, unsigned char Value);
  virtual void EmitDwarfAdvanceLineAddr(int64_t LineDelta,
                                        const MCSymbol *LastLabel,
                                        const MCSymbol *Label,
                                        unsigned PointerSize);
  virtual void EmitDwarfAdvanceFrameAddr(const MCSymbol *LastLabel,
                                         const MCSymbol *Label);
  virtual void Finish();

  /// @}
};

} // end namespace llvm

#endif
