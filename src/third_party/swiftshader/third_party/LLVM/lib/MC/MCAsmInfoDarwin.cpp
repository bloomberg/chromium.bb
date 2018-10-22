//===-- MCAsmInfoDarwin.cpp - Darwin asm properties -------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines target asm properties related what form asm statements
// should take in general on Darwin-based targets
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCAsmInfoDarwin.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
using namespace llvm;

MCAsmInfoDarwin::MCAsmInfoDarwin() {
  // Common settings for all Darwin targets.
  // Syntax:
  GlobalPrefix = "_";
  PrivateGlobalPrefix = "L";
  LinkerPrivateGlobalPrefix = "l";
  AllowQuotesInName = true;
  HasSingleParameterDotFile = false;
  HasSubsectionsViaSymbols = true;

  AlignmentIsInBytes = false;
  COMMDirectiveAlignmentIsInBytes = false;
  InlineAsmStart = " InlineAsm Start";
  InlineAsmEnd = " InlineAsm End";

  // Directives:
  WeakDefDirective = "\t.weak_definition ";
  WeakRefDirective = "\t.weak_reference ";
  ZeroDirective = "\t.space\t";  // ".space N" emits N zeros.
  HasMachoZeroFillDirective = true;  // Uses .zerofill
  HasMachoTBSSDirective = true; // Uses .tbss
  StructorOutputOrder = Structors::PriorityOrder;
  HasStaticCtorDtorReferenceInStaticMode = true;

  CodeBegin = "L$start$code$";
  DataBegin = "L$start$data$";
  JT8Begin  = "L$start$jt8$";
  JT16Begin = "L$start$jt16$";
  JT32Begin = "L$start$jt32$";
  SupportsDataRegions = true;

  // FIXME: Darwin 10 and newer don't need this.
  LinkerRequiresNonEmptyDwarfLines = true;

  // FIXME: Change this once MC is the system assembler.
  HasAggressiveSymbolFolding = false;

  HiddenVisibilityAttr = MCSA_PrivateExtern;
  HiddenDeclarationVisibilityAttr = MCSA_Invalid;
  // Doesn't support protected visibility.
  ProtectedVisibilityAttr = MCSA_Global;
  
  HasDotTypeDotSizeDirective = false;
  HasNoDeadStrip = true;
  HasSymbolResolver = true;

  DwarfRequiresRelocationForSectionOffset = false;
  DwarfUsesLabelOffsetForRanges = false;
}
