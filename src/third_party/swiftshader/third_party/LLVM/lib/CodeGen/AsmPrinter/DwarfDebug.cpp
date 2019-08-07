//===-- llvm/CodeGen/DwarfDebug.cpp - Dwarf Debug Framework ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains support for writing dwarf debug info into asm files.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "dwarfdebug"
#include "DwarfDebug.h"
#include "DIE.h"
#include "DwarfCompileUnit.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Analysis/DIBuilder.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/Path.h"
using namespace llvm;

static cl::opt<bool> DisableDebugInfoPrinting("disable-debug-info-print",
                                              cl::Hidden,
     cl::desc("Disable debug info printing"));

static cl::opt<bool> UnknownLocations("use-unknown-locations", cl::Hidden,
     cl::desc("Make an absence of debug location information explicit."),
     cl::init(false));

namespace {
  const char *DWARFGroupName = "DWARF Emission";
  const char *DbgTimerName = "DWARF Debug Writer";
} // end anonymous namespace

//===----------------------------------------------------------------------===//

/// Configuration values for initial hash set sizes (log2).
///
static const unsigned InitAbbreviationsSetSize = 9; // log2(512)

namespace llvm {

DIType DbgVariable::getType() const {
  DIType Ty = Var.getType();
  // FIXME: isBlockByrefVariable should be reformulated in terms of complex
  // addresses instead.
  if (Var.isBlockByrefVariable()) {
    /* Byref variables, in Blocks, are declared by the programmer as
       "SomeType VarName;", but the compiler creates a
       __Block_byref_x_VarName struct, and gives the variable VarName
       either the struct, or a pointer to the struct, as its type.  This
       is necessary for various behind-the-scenes things the compiler
       needs to do with by-reference variables in blocks.
       
       However, as far as the original *programmer* is concerned, the
       variable should still have type 'SomeType', as originally declared.
       
       The following function dives into the __Block_byref_x_VarName
       struct to find the original type of the variable.  This will be
       passed back to the code generating the type for the Debug
       Information Entry for the variable 'VarName'.  'VarName' will then
       have the original type 'SomeType' in its debug information.
       
       The original type 'SomeType' will be the type of the field named
       'VarName' inside the __Block_byref_x_VarName struct.
       
       NOTE: In order for this to not completely fail on the debugger
       side, the Debug Information Entry for the variable VarName needs to
       have a DW_AT_location that tells the debugger how to unwind through
       the pointers and __Block_byref_x_VarName struct to find the actual
       value of the variable.  The function addBlockByrefType does this.  */
    DIType subType = Ty;
    unsigned tag = Ty.getTag();
    
    if (tag == dwarf::DW_TAG_pointer_type) {
      DIDerivedType DTy = DIDerivedType(Ty);
      subType = DTy.getTypeDerivedFrom();
    }
    
    DICompositeType blockStruct = DICompositeType(subType);
    DIArray Elements = blockStruct.getTypeArray();
    
    for (unsigned i = 0, N = Elements.getNumElements(); i < N; ++i) {
      DIDescriptor Element = Elements.getElement(i);
      DIDerivedType DT = DIDerivedType(Element);
      if (getName() == DT.getName())
        return (DT.getTypeDerivedFrom());
    }
    return Ty;
  }
  return Ty;
}

} // end llvm namespace

DwarfDebug::DwarfDebug(AsmPrinter *A, Module *M)
  : Asm(A), MMI(Asm->MMI), FirstCU(0),
    AbbreviationsSet(InitAbbreviationsSetSize),
    PrevLabel(NULL) {
  NextStringPoolNumber = 0;

  DwarfInfoSectionSym = DwarfAbbrevSectionSym = 0;
  DwarfStrSectionSym = TextSectionSym = 0;
  DwarfDebugRangeSectionSym = DwarfDebugLocSectionSym = 0;
  FunctionBeginSym = FunctionEndSym = 0;
  {
    NamedRegionTimer T(DbgTimerName, DWARFGroupName, TimePassesIsEnabled);
    beginModule(M);
  }
}
DwarfDebug::~DwarfDebug() {
}

MCSymbol *DwarfDebug::getStringPoolEntry(StringRef Str) {
  std::pair<MCSymbol*, unsigned> &Entry = StringPool[Str];
  if (Entry.first) return Entry.first;

  Entry.second = NextStringPoolNumber++;
  return Entry.first = Asm->GetTempSymbol("string", Entry.second);
}


/// assignAbbrevNumber - Define a unique number for the abbreviation.
///
void DwarfDebug::assignAbbrevNumber(DIEAbbrev &Abbrev) {
  // Profile the node so that we can make it unique.
  FoldingSetNodeID ID;
  Abbrev.Profile(ID);

  // Check the set for priors.
  DIEAbbrev *InSet = AbbreviationsSet.GetOrInsertNode(&Abbrev);

  // If it's newly added.
  if (InSet == &Abbrev) {
    // Add to abbreviation list.
    Abbreviations.push_back(&Abbrev);

    // Assign the vector position + 1 as its number.
    Abbrev.setNumber(Abbreviations.size());
  } else {
    // Assign existing abbreviation number.
    Abbrev.setNumber(InSet->getNumber());
  }
}

/// getRealLinkageName - If special LLVM prefix that is used to inform the asm
/// printer to not emit usual symbol prefix before the symbol name is used then
/// return linkage name after skipping this special LLVM prefix.
static StringRef getRealLinkageName(StringRef LinkageName) {
  char One = '\1';
  if (LinkageName.startswith(StringRef(&One, 1)))
    return LinkageName.substr(1);
  return LinkageName;
}

/// updateSubprogramScopeDIE - Find DIE for the given subprogram and
/// attach appropriate DW_AT_low_pc and DW_AT_high_pc attributes.
/// If there are global variables in this scope then create and insert
/// DIEs for these variables.
DIE *DwarfDebug::updateSubprogramScopeDIE(CompileUnit *SPCU,
                                          const MDNode *SPNode) {
  DIE *SPDie = SPCU->getDIE(SPNode);

  assert(SPDie && "Unable to find subprogram DIE!");
  DISubprogram SP(SPNode);

  DISubprogram SPDecl = SP.getFunctionDeclaration();
  if (SPDecl.isSubprogram())
    // Refer function declaration directly.
    SPCU->addDIEEntry(SPDie, dwarf::DW_AT_specification, dwarf::DW_FORM_ref4,
                      SPCU->getOrCreateSubprogramDIE(SPDecl));
  else {
    // There is not any need to generate specification DIE for a function
    // defined at compile unit level. If a function is defined inside another
    // function then gdb prefers the definition at top level and but does not
    // expect specification DIE in parent function. So avoid creating
    // specification DIE for a function defined inside a function.
    if (SP.isDefinition() && !SP.getContext().isCompileUnit() &&
        !SP.getContext().isFile() &&
        !isSubprogramContext(SP.getContext())) {
      SPCU-> addUInt(SPDie, dwarf::DW_AT_declaration, dwarf::DW_FORM_flag, 1);
      
      // Add arguments.
      DICompositeType SPTy = SP.getType();
      DIArray Args = SPTy.getTypeArray();
      unsigned SPTag = SPTy.getTag();
      if (SPTag == dwarf::DW_TAG_subroutine_type)
        for (unsigned i = 1, N = Args.getNumElements(); i < N; ++i) {
          DIE *Arg = new DIE(dwarf::DW_TAG_formal_parameter);
          DIType ATy = DIType(DIType(Args.getElement(i)));
          SPCU->addType(Arg, ATy);
          if (ATy.isArtificial())
            SPCU->addUInt(Arg, dwarf::DW_AT_artificial, dwarf::DW_FORM_flag, 1);
          SPDie->addChild(Arg);
        }
      DIE *SPDeclDie = SPDie;
      SPDie = new DIE(dwarf::DW_TAG_subprogram);
      SPCU->addDIEEntry(SPDie, dwarf::DW_AT_specification, dwarf::DW_FORM_ref4,
                        SPDeclDie);
      SPCU->addDie(SPDie);
    }
  }
  // Pick up abstract subprogram DIE.
  if (DIE *AbsSPDIE = AbstractSPDies.lookup(SPNode)) {
    SPDie = new DIE(dwarf::DW_TAG_subprogram);
    SPCU->addDIEEntry(SPDie, dwarf::DW_AT_abstract_origin,
                      dwarf::DW_FORM_ref4, AbsSPDIE);
    SPCU->addDie(SPDie);
  }

  SPCU->addLabel(SPDie, dwarf::DW_AT_low_pc, dwarf::DW_FORM_addr,
                 Asm->GetTempSymbol("func_begin", Asm->getFunctionNumber()));
  SPCU->addLabel(SPDie, dwarf::DW_AT_high_pc, dwarf::DW_FORM_addr,
                 Asm->GetTempSymbol("func_end", Asm->getFunctionNumber()));
  const TargetRegisterInfo *RI = Asm->TM.getRegisterInfo();
  MachineLocation Location(RI->getFrameRegister(*Asm->MF));
  SPCU->addAddress(SPDie, dwarf::DW_AT_frame_base, Location);

  return SPDie;
}

/// constructLexicalScope - Construct new DW_TAG_lexical_block
/// for this scope and attach DW_AT_low_pc/DW_AT_high_pc labels.
DIE *DwarfDebug::constructLexicalScopeDIE(CompileUnit *TheCU, 
                                          LexicalScope *Scope) {

  DIE *ScopeDIE = new DIE(dwarf::DW_TAG_lexical_block);
  if (Scope->isAbstractScope())
    return ScopeDIE;

  const SmallVector<InsnRange, 4> &Ranges = Scope->getRanges();
  if (Ranges.empty())
    return 0;

  SmallVector<InsnRange, 4>::const_iterator RI = Ranges.begin();
  if (Ranges.size() > 1) {
    // .debug_range section has not been laid out yet. Emit offset in
    // .debug_range as a uint, size 4, for now. emitDIE will handle
    // DW_AT_ranges appropriately.
    TheCU->addUInt(ScopeDIE, dwarf::DW_AT_ranges, dwarf::DW_FORM_data4,
                   DebugRangeSymbols.size() 
                   * Asm->getTargetData().getPointerSize());
    for (SmallVector<InsnRange, 4>::const_iterator RI = Ranges.begin(),
         RE = Ranges.end(); RI != RE; ++RI) {
      DebugRangeSymbols.push_back(getLabelBeforeInsn(RI->first));
      DebugRangeSymbols.push_back(getLabelAfterInsn(RI->second));
    }
    DebugRangeSymbols.push_back(NULL);
    DebugRangeSymbols.push_back(NULL);
    return ScopeDIE;
  }

  const MCSymbol *Start = getLabelBeforeInsn(RI->first);
  const MCSymbol *End = getLabelAfterInsn(RI->second);

  if (End == 0) return 0;

  assert(Start->isDefined() && "Invalid starting label for an inlined scope!");
  assert(End->isDefined() && "Invalid end label for an inlined scope!");

  TheCU->addLabel(ScopeDIE, dwarf::DW_AT_low_pc, dwarf::DW_FORM_addr, Start);
  TheCU->addLabel(ScopeDIE, dwarf::DW_AT_high_pc, dwarf::DW_FORM_addr, End);

  return ScopeDIE;
}

/// constructInlinedScopeDIE - This scope represents inlined body of
/// a function. Construct DIE to represent this concrete inlined copy
/// of the function.
DIE *DwarfDebug::constructInlinedScopeDIE(CompileUnit *TheCU,
                                          LexicalScope *Scope) {

  const SmallVector<InsnRange, 4> &Ranges = Scope->getRanges();
  assert (Ranges.empty() == false
          && "LexicalScope does not have instruction markers!");

  if (!Scope->getScopeNode())
    return NULL;
  DIScope DS(Scope->getScopeNode());
  DISubprogram InlinedSP = getDISubprogram(DS);
  DIE *OriginDIE = TheCU->getDIE(InlinedSP);
  if (!OriginDIE) {
    DEBUG(dbgs() << "Unable to find original DIE for inlined subprogram.");
    return NULL;
  }

  SmallVector<InsnRange, 4>::const_iterator RI = Ranges.begin();
  const MCSymbol *StartLabel = getLabelBeforeInsn(RI->first);
  const MCSymbol *EndLabel = getLabelAfterInsn(RI->second);

  if (StartLabel == 0 || EndLabel == 0) {
    assert (0 && "Unexpected Start and End labels for a inlined scope!");
    return 0;
  }
  assert(StartLabel->isDefined() &&
         "Invalid starting label for an inlined scope!");
  assert(EndLabel->isDefined() &&
         "Invalid end label for an inlined scope!");

  DIE *ScopeDIE = new DIE(dwarf::DW_TAG_inlined_subroutine);
  TheCU->addDIEEntry(ScopeDIE, dwarf::DW_AT_abstract_origin,
                     dwarf::DW_FORM_ref4, OriginDIE);

  if (Ranges.size() > 1) {
    // .debug_range section has not been laid out yet. Emit offset in
    // .debug_range as a uint, size 4, for now. emitDIE will handle
    // DW_AT_ranges appropriately.
    TheCU->addUInt(ScopeDIE, dwarf::DW_AT_ranges, dwarf::DW_FORM_data4,
                   DebugRangeSymbols.size() 
                   * Asm->getTargetData().getPointerSize());
    for (SmallVector<InsnRange, 4>::const_iterator RI = Ranges.begin(),
         RE = Ranges.end(); RI != RE; ++RI) {
      DebugRangeSymbols.push_back(getLabelBeforeInsn(RI->first));
      DebugRangeSymbols.push_back(getLabelAfterInsn(RI->second));
    }
    DebugRangeSymbols.push_back(NULL);
    DebugRangeSymbols.push_back(NULL);
  } else {
    TheCU->addLabel(ScopeDIE, dwarf::DW_AT_low_pc, dwarf::DW_FORM_addr, 
                    StartLabel);
    TheCU->addLabel(ScopeDIE, dwarf::DW_AT_high_pc, dwarf::DW_FORM_addr, 
                    EndLabel);
  }

  InlinedSubprogramDIEs.insert(OriginDIE);

  // Track the start label for this inlined function.
  //.debug_inlined section specification does not clearly state how
  // to emit inlined scope that is split into multiple instruction ranges.
  // For now, use first instruction range and emit low_pc/high_pc pair and
  // corresponding .debug_inlined section entry for this pair.
  DenseMap<const MDNode *, SmallVector<InlineInfoLabels, 4> >::iterator
    I = InlineInfo.find(InlinedSP);

  if (I == InlineInfo.end()) {
    InlineInfo[InlinedSP].push_back(std::make_pair(StartLabel,
                                                             ScopeDIE));
    InlinedSPNodes.push_back(InlinedSP);
  } else
    I->second.push_back(std::make_pair(StartLabel, ScopeDIE));

  DILocation DL(Scope->getInlinedAt());
  TheCU->addUInt(ScopeDIE, dwarf::DW_AT_call_file, 0, TheCU->getID());
  TheCU->addUInt(ScopeDIE, dwarf::DW_AT_call_line, 0, DL.getLineNumber());

  return ScopeDIE;
}

/// constructScopeDIE - Construct a DIE for this scope.
DIE *DwarfDebug::constructScopeDIE(CompileUnit *TheCU, LexicalScope *Scope) {
  if (!Scope || !Scope->getScopeNode())
    return NULL;

  SmallVector <DIE *, 8> Children;

  // Collect arguments for current function.
  if (LScopes.isCurrentFunctionScope(Scope))
    for (unsigned i = 0, N = CurrentFnArguments.size(); i < N; ++i)
      if (DbgVariable *ArgDV = CurrentFnArguments[i])
        if (DIE *Arg = 
            TheCU->constructVariableDIE(ArgDV, Scope->isAbstractScope()))
          Children.push_back(Arg);

  // Collect lexical scope children first.
  const SmallVector<DbgVariable *, 8> &Variables = ScopeVariables.lookup(Scope);
  for (unsigned i = 0, N = Variables.size(); i < N; ++i)
    if (DIE *Variable = 
        TheCU->constructVariableDIE(Variables[i], Scope->isAbstractScope()))
      Children.push_back(Variable);
  const SmallVector<LexicalScope *, 4> &Scopes = Scope->getChildren();
  for (unsigned j = 0, M = Scopes.size(); j < M; ++j)
    if (DIE *Nested = constructScopeDIE(TheCU, Scopes[j]))
      Children.push_back(Nested);
  DIScope DS(Scope->getScopeNode());
  DIE *ScopeDIE = NULL;
  if (Scope->getInlinedAt())
    ScopeDIE = constructInlinedScopeDIE(TheCU, Scope);
  else if (DS.isSubprogram()) {
    ProcessedSPNodes.insert(DS);
    if (Scope->isAbstractScope()) {
      ScopeDIE = TheCU->getDIE(DS);
      // Note down abstract DIE.
      if (ScopeDIE)
        AbstractSPDies.insert(std::make_pair(DS, ScopeDIE));
    }
    else
      ScopeDIE = updateSubprogramScopeDIE(TheCU, DS);
  }
  else {
    // There is no need to emit empty lexical block DIE.
    if (Children.empty())
      return NULL;
    ScopeDIE = constructLexicalScopeDIE(TheCU, Scope);
  }
  
  if (!ScopeDIE) return NULL;

  // Add children
  for (SmallVector<DIE *, 8>::iterator I = Children.begin(),
         E = Children.end(); I != E; ++I)
    ScopeDIE->addChild(*I);

  if (DS.isSubprogram())
   TheCU->addPubTypes(DISubprogram(DS));

 return ScopeDIE;
}

/// GetOrCreateSourceID - Look up the source id with the given directory and
/// source file names. If none currently exists, create a new id and insert it
/// in the SourceIds map. This can update DirectoryNames and SourceFileNames
/// maps as well.

unsigned DwarfDebug::GetOrCreateSourceID(StringRef FileName, 
                                         StringRef DirName) {
  // If FE did not provide a file name, then assume stdin.
  if (FileName.empty())
    return GetOrCreateSourceID("<stdin>", StringRef());

  // MCStream expects full path name as filename.
  if (!DirName.empty() && !sys::path::is_absolute(FileName)) {
    SmallString<128> FullPathName = DirName;
    sys::path::append(FullPathName, FileName);
    // Here FullPathName will be copied into StringMap by GetOrCreateSourceID.
    return GetOrCreateSourceID(StringRef(FullPathName), StringRef());
  }

  StringMapEntry<unsigned> &Entry = SourceIdMap.GetOrCreateValue(FileName);
  if (Entry.getValue())
    return Entry.getValue();

  unsigned SrcId = SourceIdMap.size();
  Entry.setValue(SrcId);

  // Print out a .file directive to specify files for .loc directives.
  Asm->OutStreamer.EmitDwarfFileDirective(SrcId, Entry.getKey());

  return SrcId;
}

/// constructCompileUnit - Create new CompileUnit for the given
/// metadata node with tag DW_TAG_compile_unit.
CompileUnit *DwarfDebug::constructCompileUnit(const MDNode *N) {
  DICompileUnit DIUnit(N);
  StringRef FN = DIUnit.getFilename();
  StringRef Dir = DIUnit.getDirectory();
  unsigned ID = GetOrCreateSourceID(FN, Dir);

  DIE *Die = new DIE(dwarf::DW_TAG_compile_unit);
  CompileUnit *NewCU = new CompileUnit(ID, Die, Asm, this);
  NewCU->addString(Die, dwarf::DW_AT_producer, dwarf::DW_FORM_string,
                   DIUnit.getProducer());
  NewCU->addUInt(Die, dwarf::DW_AT_language, dwarf::DW_FORM_data2,
                 DIUnit.getLanguage());
  NewCU->addString(Die, dwarf::DW_AT_name, dwarf::DW_FORM_string, FN);
  // Use DW_AT_entry_pc instead of DW_AT_low_pc/DW_AT_high_pc pair. This
  // simplifies debug range entries.
  NewCU->addUInt(Die, dwarf::DW_AT_entry_pc, dwarf::DW_FORM_addr, 0);
  // DW_AT_stmt_list is a offset of line number information for this
  // compile unit in debug_line section.
  if(Asm->MAI->doesDwarfRequireRelocationForSectionOffset())
    NewCU->addLabel(Die, dwarf::DW_AT_stmt_list, dwarf::DW_FORM_data4,
                    Asm->GetTempSymbol("section_line"));
  else
    NewCU->addUInt(Die, dwarf::DW_AT_stmt_list, dwarf::DW_FORM_data4, 0);

  if (!Dir.empty())
    NewCU->addString(Die, dwarf::DW_AT_comp_dir, dwarf::DW_FORM_string, Dir);
  if (DIUnit.isOptimized())
    NewCU->addUInt(Die, dwarf::DW_AT_APPLE_optimized, dwarf::DW_FORM_flag, 1);

  StringRef Flags = DIUnit.getFlags();
  if (!Flags.empty())
    NewCU->addString(Die, dwarf::DW_AT_APPLE_flags, dwarf::DW_FORM_string, 
                     Flags);
  
  unsigned RVer = DIUnit.getRunTimeVersion();
  if (RVer)
    NewCU->addUInt(Die, dwarf::DW_AT_APPLE_major_runtime_vers,
            dwarf::DW_FORM_data1, RVer);

  if (!FirstCU)
    FirstCU = NewCU;
  CUMap.insert(std::make_pair(N, NewCU));
  return NewCU;
}

/// construct SubprogramDIE - Construct subprogram DIE.
void DwarfDebug::constructSubprogramDIE(CompileUnit *TheCU, 
                                        const MDNode *N) {
  DISubprogram SP(N);
  if (!SP.isDefinition())
    // This is a method declaration which will be handled while constructing
    // class type.
    return;

  DIE *SubprogramDie = TheCU->getOrCreateSubprogramDIE(SP);

  // Add to map.
  TheCU->insertDIE(N, SubprogramDie);

  // Add to context owner.
  TheCU->addToContextOwner(SubprogramDie, SP.getContext());

  // Expose as global.
  TheCU->addGlobal(SP.getName(), SubprogramDie);

  SPMap[N] = TheCU;
  return;
}

/// collectInfoFromNamedMDNodes - Collect debug info from named mdnodes such
/// as llvm.dbg.enum and llvm.dbg.ty
void DwarfDebug::collectInfoFromNamedMDNodes(Module *M) {
  if (NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.sp"))
    for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
      const MDNode *N = NMD->getOperand(i);
      if (CompileUnit *CU = CUMap.lookup(DISubprogram(N).getCompileUnit()))
        constructSubprogramDIE(CU, N);
    }
  
  if (NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.gv"))
    for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
      const MDNode *N = NMD->getOperand(i);
      if (CompileUnit *CU = CUMap.lookup(DIGlobalVariable(N).getCompileUnit()))
        CU->createGlobalVariableDIE(N);
    }
  
  if (NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.enum"))
    for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
      DIType Ty(NMD->getOperand(i));
      if (CompileUnit *CU = CUMap.lookup(Ty.getCompileUnit()))
        CU->getOrCreateTypeDIE(Ty);
    }
  
  if (NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.ty"))
    for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
      DIType Ty(NMD->getOperand(i));
      if (CompileUnit *CU = CUMap.lookup(Ty.getCompileUnit()))
        CU->getOrCreateTypeDIE(Ty);
    }
}

/// collectLegacyDebugInfo - Collect debug info using DebugInfoFinder.
/// FIXME - Remove this when dragon-egg and llvm-gcc switch to DIBuilder.
bool DwarfDebug::collectLegacyDebugInfo(Module *M) {
  DebugInfoFinder DbgFinder;
  DbgFinder.processModule(*M);
  
  bool HasDebugInfo = false;
  // Scan all the compile-units to see if there are any marked as the main
  // unit. If not, we do not generate debug info.
  for (DebugInfoFinder::iterator I = DbgFinder.compile_unit_begin(),
         E = DbgFinder.compile_unit_end(); I != E; ++I) {
    if (DICompileUnit(*I).isMain()) {
      HasDebugInfo = true;
      break;
    }
  }
  if (!HasDebugInfo) return false;
  
  // Create all the compile unit DIEs.
  for (DebugInfoFinder::iterator I = DbgFinder.compile_unit_begin(),
         E = DbgFinder.compile_unit_end(); I != E; ++I)
    constructCompileUnit(*I);
  
  // Create DIEs for each global variable.
  for (DebugInfoFinder::iterator I = DbgFinder.global_variable_begin(),
         E = DbgFinder.global_variable_end(); I != E; ++I) {
    const MDNode *N = *I;
    if (CompileUnit *CU = CUMap.lookup(DIGlobalVariable(N).getCompileUnit()))
      CU->createGlobalVariableDIE(N);
  }
    
  // Create DIEs for each subprogram.
  for (DebugInfoFinder::iterator I = DbgFinder.subprogram_begin(),
         E = DbgFinder.subprogram_end(); I != E; ++I) {
    const MDNode *N = *I;
    if (CompileUnit *CU = CUMap.lookup(DISubprogram(N).getCompileUnit()))
      constructSubprogramDIE(CU, N);
  }

  return HasDebugInfo;
}

/// beginModule - Emit all Dwarf sections that should come prior to the
/// content. Create global DIEs and emit initial debug info sections.
/// This is invoked by the target AsmPrinter.
void DwarfDebug::beginModule(Module *M) {
  if (DisableDebugInfoPrinting)
    return;

  // If module has named metadata anchors then use them, otherwise scan the
  // module using debug info finder to collect debug info.
  NamedMDNode *CU_Nodes = M->getNamedMetadata("llvm.dbg.cu");
  if (CU_Nodes) {
    for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i) {
      DICompileUnit CUNode(CU_Nodes->getOperand(i));
      CompileUnit *CU = constructCompileUnit(CUNode);
      DIArray GVs = CUNode.getGlobalVariables();
      for (unsigned i = 0, e = GVs.getNumElements(); i != e; ++i)
        CU->createGlobalVariableDIE(GVs.getElement(i));
      DIArray SPs = CUNode.getSubprograms();
      for (unsigned i = 0, e = SPs.getNumElements(); i != e; ++i)
        constructSubprogramDIE(CU, SPs.getElement(i));
      DIArray EnumTypes = CUNode.getEnumTypes();
      for (unsigned i = 0, e = EnumTypes.getNumElements(); i != e; ++i)
        CU->getOrCreateTypeDIE(EnumTypes.getElement(i));
      DIArray RetainedTypes = CUNode.getRetainedTypes();
      for (unsigned i = 0, e = RetainedTypes.getNumElements(); i != e; ++i)
        CU->getOrCreateTypeDIE(RetainedTypes.getElement(i));
    }
  } else if (!collectLegacyDebugInfo(M))
    return;

  collectInfoFromNamedMDNodes(M);
  
  // Tell MMI that we have debug info.
  MMI->setDebugInfoAvailability(true);
  
  // Emit initial sections.
  EmitSectionLabels();

  // Prime section data.
  SectionMap.insert(Asm->getObjFileLowering().getTextSection());
}

/// endModule - Emit all Dwarf sections that should come after the content.
///
void DwarfDebug::endModule() {
  if (!FirstCU) return;
  const Module *M = MMI->getModule();
  DenseMap<const MDNode *, LexicalScope *> DeadFnScopeMap;

  // Collect info for variables that were optimized out.
  if (NamedMDNode *CU_Nodes = M->getNamedMetadata("llvm.dbg.cu")) {
    for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i) {
      DICompileUnit TheCU(CU_Nodes->getOperand(i));
      DIArray Subprograms = TheCU.getSubprograms();
      for (unsigned i = 0, e = Subprograms.getNumElements(); i != e; ++i) {
        DISubprogram SP(Subprograms.getElement(i));
        if (ProcessedSPNodes.count(SP) != 0) continue;
        if (!SP.Verify()) continue;
        if (!SP.isDefinition()) continue;
        DIArray Variables = SP.getVariables();
        if (Variables.getNumElements() == 0) continue;

        LexicalScope *Scope = 
          new LexicalScope(NULL, DIDescriptor(SP), NULL, false);
        DeadFnScopeMap[SP] = Scope;
        
        // Construct subprogram DIE and add variables DIEs.
        CompileUnit *SPCU = CUMap.lookup(TheCU);
        assert (SPCU && "Unable to find Compile Unit!");
        constructSubprogramDIE(SPCU, SP);
        DIE *ScopeDIE = SPCU->getDIE(SP);
        for (unsigned vi = 0, ve = Variables.getNumElements(); vi != ve; ++vi) {
          DIVariable DV(Variables.getElement(vi));
          if (!DV.Verify()) continue;
          DbgVariable *NewVar = new DbgVariable(DV, NULL);
          if (DIE *VariableDIE = 
              SPCU->constructVariableDIE(NewVar, Scope->isAbstractScope()))
            ScopeDIE->addChild(VariableDIE);
        }
      }
    }
  }

  // Attach DW_AT_inline attribute with inlined subprogram DIEs.
  for (SmallPtrSet<DIE *, 4>::iterator AI = InlinedSubprogramDIEs.begin(),
         AE = InlinedSubprogramDIEs.end(); AI != AE; ++AI) {
    DIE *ISP = *AI;
    FirstCU->addUInt(ISP, dwarf::DW_AT_inline, 0, dwarf::DW_INL_inlined);
  }

  // Emit DW_AT_containing_type attribute to connect types with their
  // vtable holding type.
  for (DenseMap<const MDNode *, CompileUnit *>::iterator CUI = CUMap.begin(),
         CUE = CUMap.end(); CUI != CUE; ++CUI) {
    CompileUnit *TheCU = CUI->second;
    TheCU->constructContainingTypeDIEs();
  }

  // Standard sections final addresses.
  Asm->OutStreamer.SwitchSection(Asm->getObjFileLowering().getTextSection());
  Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("text_end"));
  Asm->OutStreamer.SwitchSection(Asm->getObjFileLowering().getDataSection());
  Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("data_end"));

  // End text sections.
  for (unsigned i = 1, N = SectionMap.size(); i <= N; ++i) {
    Asm->OutStreamer.SwitchSection(SectionMap[i]);
    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("section_end", i));
  }

  // Compute DIE offsets and sizes.
  computeSizeAndOffsets();

  // Emit all the DIEs into a debug info section
  emitDebugInfo();

  // Corresponding abbreviations into a abbrev section.
  emitAbbreviations();

  // Emit info into a debug pubnames section.
  emitDebugPubNames();

  // Emit info into a debug pubtypes section.
  emitDebugPubTypes();

  // Emit info into a debug loc section.
  emitDebugLoc();

  // Emit info into a debug aranges section.
  EmitDebugARanges();

  // Emit info into a debug ranges section.
  emitDebugRanges();

  // Emit info into a debug macinfo section.
  emitDebugMacInfo();

  // Emit inline info.
  emitDebugInlineInfo();

  // Emit info into a debug str section.
  emitDebugStr();

  // clean up.
  DeleteContainerSeconds(DeadFnScopeMap);
  SPMap.clear();
  for (DenseMap<const MDNode *, CompileUnit *>::iterator I = CUMap.begin(),
         E = CUMap.end(); I != E; ++I)
    delete I->second;
  FirstCU = NULL;  // Reset for the next Module, if any.
}

/// findAbstractVariable - Find abstract variable, if any, associated with Var.
DbgVariable *DwarfDebug::findAbstractVariable(DIVariable &DV,
                                              DebugLoc ScopeLoc) {
  LLVMContext &Ctx = DV->getContext();
  // More then one inlined variable corresponds to one abstract variable.
  DIVariable Var = cleanseInlinedVariable(DV, Ctx);
  DbgVariable *AbsDbgVariable = AbstractVariables.lookup(Var);
  if (AbsDbgVariable)
    return AbsDbgVariable;

  LexicalScope *Scope = LScopes.findAbstractScope(ScopeLoc.getScope(Ctx));
  if (!Scope)
    return NULL;

  AbsDbgVariable = new DbgVariable(Var, NULL);
  addScopeVariable(Scope, AbsDbgVariable);
  AbstractVariables[Var] = AbsDbgVariable;
  return AbsDbgVariable;
}

/// addCurrentFnArgument - If Var is a current function argument then add
/// it to CurrentFnArguments list.
bool DwarfDebug::addCurrentFnArgument(const MachineFunction *MF,
                                      DbgVariable *Var, LexicalScope *Scope) {
  if (!LScopes.isCurrentFunctionScope(Scope))
    return false;
  DIVariable DV = Var->getVariable();
  if (DV.getTag() != dwarf::DW_TAG_arg_variable)
    return false;
  unsigned ArgNo = DV.getArgNumber();
  if (ArgNo == 0) 
    return false;

  size_t Size = CurrentFnArguments.size();
  if (Size == 0)
    CurrentFnArguments.resize(MF->getFunction()->arg_size());
  // llvm::Function argument size is not good indicator of how many
  // arguments does the function have at source level.
  if (ArgNo > Size)
    CurrentFnArguments.resize(ArgNo * 2);
  CurrentFnArguments[ArgNo - 1] = Var;
  return true;
}

/// collectVariableInfoFromMMITable - Collect variable information from
/// side table maintained by MMI.
void
DwarfDebug::collectVariableInfoFromMMITable(const MachineFunction *MF,
                                   SmallPtrSet<const MDNode *, 16> &Processed) {
  MachineModuleInfo::VariableDbgInfoMapTy &VMap = MMI->getVariableDbgInfo();
  for (MachineModuleInfo::VariableDbgInfoMapTy::iterator VI = VMap.begin(),
         VE = VMap.end(); VI != VE; ++VI) {
    const MDNode *Var = VI->first;
    if (!Var) continue;
    Processed.insert(Var);
    DIVariable DV(Var);
    const std::pair<unsigned, DebugLoc> &VP = VI->second;

    LexicalScope *Scope = LScopes.findLexicalScope(VP.second);

    // If variable scope is not found then skip this variable.
    if (Scope == 0)
      continue;

    DbgVariable *AbsDbgVariable = findAbstractVariable(DV, VP.second);
    DbgVariable *RegVar = new DbgVariable(DV, AbsDbgVariable);
    RegVar->setFrameIndex(VP.first);
    if (!addCurrentFnArgument(MF, RegVar, Scope))
      addScopeVariable(Scope, RegVar);
    if (AbsDbgVariable)
      AbsDbgVariable->setFrameIndex(VP.first);
  }
}

/// isDbgValueInDefinedReg - Return true if debug value, encoded by
/// DBG_VALUE instruction, is in a defined reg.
static bool isDbgValueInDefinedReg(const MachineInstr *MI) {
  assert (MI->isDebugValue() && "Invalid DBG_VALUE machine instruction!");
  return MI->getNumOperands() == 3 &&
         MI->getOperand(0).isReg() && MI->getOperand(0).getReg() &&
         MI->getOperand(1).isImm() && MI->getOperand(1).getImm() == 0;
}

/// getDebugLocEntry - Get .debug_loc entry for the instruction range starting
/// at MI.
static DotDebugLocEntry getDebugLocEntry(AsmPrinter *Asm, 
                                         const MCSymbol *FLabel, 
                                         const MCSymbol *SLabel,
                                         const MachineInstr *MI) {
  const MDNode *Var =  MI->getOperand(MI->getNumOperands() - 1).getMetadata();

  if (MI->getNumOperands() != 3) {
    MachineLocation MLoc = Asm->getDebugValueLocation(MI);
    return DotDebugLocEntry(FLabel, SLabel, MLoc, Var);
  }
  if (MI->getOperand(0).isReg() && MI->getOperand(1).isImm()) {
    MachineLocation MLoc;
    MLoc.set(MI->getOperand(0).getReg(), MI->getOperand(1).getImm());
    return DotDebugLocEntry(FLabel, SLabel, MLoc, Var);
  }
  if (MI->getOperand(0).isImm())
    return DotDebugLocEntry(FLabel, SLabel, MI->getOperand(0).getImm());
  if (MI->getOperand(0).isFPImm())
    return DotDebugLocEntry(FLabel, SLabel, MI->getOperand(0).getFPImm());
  if (MI->getOperand(0).isCImm())
    return DotDebugLocEntry(FLabel, SLabel, MI->getOperand(0).getCImm());

  assert (0 && "Unexpected 3 operand DBG_VALUE instruction!");
  return DotDebugLocEntry();
}

/// collectVariableInfo - Find variables for each lexical scope.
void
DwarfDebug::collectVariableInfo(const MachineFunction *MF,
                                SmallPtrSet<const MDNode *, 16> &Processed) {

  /// collection info from MMI table.
  collectVariableInfoFromMMITable(MF, Processed);

  for (SmallVectorImpl<const MDNode*>::const_iterator
         UVI = UserVariables.begin(), UVE = UserVariables.end(); UVI != UVE;
         ++UVI) {
    const MDNode *Var = *UVI;
    if (Processed.count(Var))
      continue;

    // History contains relevant DBG_VALUE instructions for Var and instructions
    // clobbering it.
    SmallVectorImpl<const MachineInstr*> &History = DbgValues[Var];
    if (History.empty())
      continue;
    const MachineInstr *MInsn = History.front();

    DIVariable DV(Var);
    LexicalScope *Scope = NULL;
    if (DV.getTag() == dwarf::DW_TAG_arg_variable &&
        DISubprogram(DV.getContext()).describes(MF->getFunction()))
      Scope = LScopes.getCurrentFunctionScope();
    else {
      if (DV.getVersion() <= LLVMDebugVersion9)
        Scope = LScopes.findLexicalScope(MInsn->getDebugLoc());
      else {
        if (MDNode *IA = DV.getInlinedAt())
          Scope = LScopes.findInlinedScope(DebugLoc::getFromDILocation(IA));
        else
          Scope = LScopes.findLexicalScope(cast<MDNode>(DV->getOperand(1)));
      }
    }
    // If variable scope is not found then skip this variable.
    if (!Scope)
      continue;

    Processed.insert(DV);
    assert(MInsn->isDebugValue() && "History must begin with debug value");
    DbgVariable *AbsVar = findAbstractVariable(DV, MInsn->getDebugLoc());
    DbgVariable *RegVar = new DbgVariable(DV, AbsVar);
    if (!addCurrentFnArgument(MF, RegVar, Scope))
      addScopeVariable(Scope, RegVar);
    if (AbsVar)
      AbsVar->setMInsn(MInsn);

    // Simple ranges that are fully coalesced.
    if (History.size() <= 1 || (History.size() == 2 &&
                                MInsn->isIdenticalTo(History.back()))) {
      RegVar->setMInsn(MInsn);
      continue;
    }

    // handle multiple DBG_VALUE instructions describing one variable.
    RegVar->setDotDebugLocOffset(DotDebugLocEntries.size());

    for (SmallVectorImpl<const MachineInstr*>::const_iterator
           HI = History.begin(), HE = History.end(); HI != HE; ++HI) {
      const MachineInstr *Begin = *HI;
      assert(Begin->isDebugValue() && "Invalid History entry");

      // Check if DBG_VALUE is truncating a range.
      if (Begin->getNumOperands() > 1 && Begin->getOperand(0).isReg()
          && !Begin->getOperand(0).getReg())
        continue;

      // Compute the range for a register location.
      const MCSymbol *FLabel = getLabelBeforeInsn(Begin);
      const MCSymbol *SLabel = 0;

      if (HI + 1 == HE)
        // If Begin is the last instruction in History then its value is valid
        // until the end of the function.
        SLabel = FunctionEndSym;
      else {
        const MachineInstr *End = HI[1];
        DEBUG(dbgs() << "DotDebugLoc Pair:\n" 
              << "\t" << *Begin << "\t" << *End << "\n");
        if (End->isDebugValue())
          SLabel = getLabelBeforeInsn(End);
        else {
          // End is a normal instruction clobbering the range.
          SLabel = getLabelAfterInsn(End);
          assert(SLabel && "Forgot label after clobber instruction");
          ++HI;
        }
      }

      // The value is valid until the next DBG_VALUE or clobber.
      DotDebugLocEntries.push_back(getDebugLocEntry(Asm, FLabel, SLabel, Begin));
    }
    DotDebugLocEntries.push_back(DotDebugLocEntry());
  }

  // Collect info for variables that were optimized out.
  LexicalScope *FnScope = LScopes.getCurrentFunctionScope();
  DIArray Variables = DISubprogram(FnScope->getScopeNode()).getVariables();
  for (unsigned i = 0, e = Variables.getNumElements(); i != e; ++i) {
    DIVariable DV(Variables.getElement(i));
    if (!DV || !DV.Verify() || !Processed.insert(DV))
      continue;
    if (LexicalScope *Scope = LScopes.findLexicalScope(DV.getContext()))
      addScopeVariable(Scope, new DbgVariable(DV, NULL));
  }
}

/// getLabelBeforeInsn - Return Label preceding the instruction.
const MCSymbol *DwarfDebug::getLabelBeforeInsn(const MachineInstr *MI) {
  MCSymbol *Label = LabelsBeforeInsn.lookup(MI);
  assert(Label && "Didn't insert label before instruction");
  return Label;
}

/// getLabelAfterInsn - Return Label immediately following the instruction.
const MCSymbol *DwarfDebug::getLabelAfterInsn(const MachineInstr *MI) {
  return LabelsAfterInsn.lookup(MI);
}

/// beginInstruction - Process beginning of an instruction.
void DwarfDebug::beginInstruction(const MachineInstr *MI) {
  // Check if source location changes, but ignore DBG_VALUE locations.
  if (!MI->isDebugValue()) {
    DebugLoc DL = MI->getDebugLoc();
    if (DL != PrevInstLoc && (!DL.isUnknown() || UnknownLocations)) {
      unsigned Flags = DWARF2_FLAG_IS_STMT;
      PrevInstLoc = DL;
      if (DL == PrologEndLoc) {
        Flags |= DWARF2_FLAG_PROLOGUE_END;
        PrologEndLoc = DebugLoc();
      }
      if (!DL.isUnknown()) {
        const MDNode *Scope = DL.getScope(Asm->MF->getFunction()->getContext());
        recordSourceLine(DL.getLine(), DL.getCol(), Scope, Flags);
      } else
        recordSourceLine(0, 0, 0, 0);
    }
  }

  // Insert labels where requested.
  DenseMap<const MachineInstr*, MCSymbol*>::iterator I =
    LabelsBeforeInsn.find(MI);

  // No label needed.
  if (I == LabelsBeforeInsn.end())
    return;

  // Label already assigned.
  if (I->second)
    return;

  if (!PrevLabel) {
    PrevLabel = MMI->getContext().CreateTempSymbol();
    Asm->OutStreamer.EmitLabel(PrevLabel);
  }
  I->second = PrevLabel;
}

/// endInstruction - Process end of an instruction.
void DwarfDebug::endInstruction(const MachineInstr *MI) {
  // Don't create a new label after DBG_VALUE instructions.
  // They don't generate code.
  if (!MI->isDebugValue())
    PrevLabel = 0;

  DenseMap<const MachineInstr*, MCSymbol*>::iterator I =
    LabelsAfterInsn.find(MI);

  // No label needed.
  if (I == LabelsAfterInsn.end())
    return;

  // Label already assigned.
  if (I->second)
    return;

  // We need a label after this instruction.
  if (!PrevLabel) {
    PrevLabel = MMI->getContext().CreateTempSymbol();
    Asm->OutStreamer.EmitLabel(PrevLabel);
  }
  I->second = PrevLabel;
}

/// identifyScopeMarkers() -
/// Each LexicalScope has first instruction and last instruction to mark
/// beginning and end of a scope respectively. Create an inverse map that list
/// scopes starts (and ends) with an instruction. One instruction may start (or
/// end) multiple scopes. Ignore scopes that are not reachable.
void DwarfDebug::identifyScopeMarkers() {
  SmallVector<LexicalScope *, 4> WorkList;
  WorkList.push_back(LScopes.getCurrentFunctionScope());
  while (!WorkList.empty()) {
    LexicalScope *S = WorkList.pop_back_val();

    const SmallVector<LexicalScope *, 4> &Children = S->getChildren();
    if (!Children.empty())
      for (SmallVector<LexicalScope *, 4>::const_iterator SI = Children.begin(),
             SE = Children.end(); SI != SE; ++SI)
        WorkList.push_back(*SI);

    if (S->isAbstractScope())
      continue;

    const SmallVector<InsnRange, 4> &Ranges = S->getRanges();
    if (Ranges.empty())
      continue;
    for (SmallVector<InsnRange, 4>::const_iterator RI = Ranges.begin(),
           RE = Ranges.end(); RI != RE; ++RI) {
      assert(RI->first && "InsnRange does not have first instruction!");
      assert(RI->second && "InsnRange does not have second instruction!");
      requestLabelBeforeInsn(RI->first);
      requestLabelAfterInsn(RI->second);
    }
  }
}

/// getScopeNode - Get MDNode for DebugLoc's scope.
static MDNode *getScopeNode(DebugLoc DL, const LLVMContext &Ctx) {
  if (MDNode *InlinedAt = DL.getInlinedAt(Ctx))
    return getScopeNode(DebugLoc::getFromDILocation(InlinedAt), Ctx);
  return DL.getScope(Ctx);
}

/// getFnDebugLoc - Walk up the scope chain of given debug loc and find
/// line number  info for the function.
static DebugLoc getFnDebugLoc(DebugLoc DL, const LLVMContext &Ctx) {
  const MDNode *Scope = getScopeNode(DL, Ctx);
  DISubprogram SP = getDISubprogram(Scope);
  if (SP.Verify()) 
    return DebugLoc::get(SP.getLineNumber(), 0, SP);
  return DebugLoc();
}

/// beginFunction - Gather pre-function debug information.  Assumes being
/// emitted immediately after the function entry point.
void DwarfDebug::beginFunction(const MachineFunction *MF) {
  if (!MMI->hasDebugInfo()) return;
  LScopes.initialize(*MF);
  if (LScopes.empty()) return;
  identifyScopeMarkers();

  FunctionBeginSym = Asm->GetTempSymbol("func_begin",
                                        Asm->getFunctionNumber());
  // Assumes in correct section after the entry point.
  Asm->OutStreamer.EmitLabel(FunctionBeginSym);

  assert(UserVariables.empty() && DbgValues.empty() && "Maps weren't cleaned");

  const TargetRegisterInfo *TRI = Asm->TM.getRegisterInfo();
  /// LiveUserVar - Map physreg numbers to the MDNode they contain.
  std::vector<const MDNode*> LiveUserVar(TRI->getNumRegs());

  for (MachineFunction::const_iterator I = MF->begin(), E = MF->end();
       I != E; ++I) {
    bool AtBlockEntry = true;
    for (MachineBasicBlock::const_iterator II = I->begin(), IE = I->end();
         II != IE; ++II) {
      const MachineInstr *MI = II;

      if (MI->isDebugValue()) {
        assert (MI->getNumOperands() > 1 && "Invalid machine instruction!");

        // Keep track of user variables.
        const MDNode *Var =
          MI->getOperand(MI->getNumOperands() - 1).getMetadata();

        // Variable is in a register, we need to check for clobbers.
        if (isDbgValueInDefinedReg(MI))
          LiveUserVar[MI->getOperand(0).getReg()] = Var;

        // Check the history of this variable.
        SmallVectorImpl<const MachineInstr*> &History = DbgValues[Var];
        if (History.empty()) {
          UserVariables.push_back(Var);
          // The first mention of a function argument gets the FunctionBeginSym
          // label, so arguments are visible when breaking at function entry.
          DIVariable DV(Var);
          if (DV.Verify() && DV.getTag() == dwarf::DW_TAG_arg_variable &&
              DISubprogram(getDISubprogram(DV.getContext()))
                .describes(MF->getFunction()))
            LabelsBeforeInsn[MI] = FunctionBeginSym;
        } else {
          // We have seen this variable before. Try to coalesce DBG_VALUEs.
          const MachineInstr *Prev = History.back();
          if (Prev->isDebugValue()) {
            // Coalesce identical entries at the end of History.
            if (History.size() >= 2 &&
                Prev->isIdenticalTo(History[History.size() - 2])) {
              DEBUG(dbgs() << "Coalesce identical DBG_VALUE entries:\n"
                    << "\t" << *Prev 
                    << "\t" << *History[History.size() - 2] << "\n");
              History.pop_back();
            }

            // Terminate old register assignments that don't reach MI;
            MachineFunction::const_iterator PrevMBB = Prev->getParent();
            if (PrevMBB != I && (!AtBlockEntry || llvm::next(PrevMBB) != I) &&
                isDbgValueInDefinedReg(Prev)) {
              // Previous register assignment needs to terminate at the end of
              // its basic block.
              MachineBasicBlock::const_iterator LastMI =
                PrevMBB->getLastNonDebugInstr();
              if (LastMI == PrevMBB->end()) {
                // Drop DBG_VALUE for empty range.
                DEBUG(dbgs() << "Drop DBG_VALUE for empty range:\n"
                      << "\t" << *Prev << "\n");
                History.pop_back();
              }
              else {
                // Terminate after LastMI.
                History.push_back(LastMI);
              }
            }
          }
        }
        History.push_back(MI);
      } else {
        // Not a DBG_VALUE instruction.
        if (!MI->isLabel())
          AtBlockEntry = false;

        // First known non DBG_VALUE location marks beginning of function
        // body.
        if (PrologEndLoc.isUnknown() && !MI->getDebugLoc().isUnknown())
          PrologEndLoc = MI->getDebugLoc();

        // Check if the instruction clobbers any registers with debug vars.
        for (MachineInstr::const_mop_iterator MOI = MI->operands_begin(),
               MOE = MI->operands_end(); MOI != MOE; ++MOI) {
          if (!MOI->isReg() || !MOI->isDef() || !MOI->getReg())
            continue;
          for (const unsigned *AI = TRI->getOverlaps(MOI->getReg());
               unsigned Reg = *AI; ++AI) {
            const MDNode *Var = LiveUserVar[Reg];
            if (!Var)
              continue;
            // Reg is now clobbered.
            LiveUserVar[Reg] = 0;

            // Was MD last defined by a DBG_VALUE referring to Reg?
            DbgValueHistoryMap::iterator HistI = DbgValues.find(Var);
            if (HistI == DbgValues.end())
              continue;
            SmallVectorImpl<const MachineInstr*> &History = HistI->second;
            if (History.empty())
              continue;
            const MachineInstr *Prev = History.back();
            // Sanity-check: Register assignments are terminated at the end of
            // their block.
            if (!Prev->isDebugValue() || Prev->getParent() != MI->getParent())
              continue;
            // Is the variable still in Reg?
            if (!isDbgValueInDefinedReg(Prev) ||
                Prev->getOperand(0).getReg() != Reg)
              continue;
            // Var is clobbered. Make sure the next instruction gets a label.
            History.push_back(MI);
          }
        }
      }
    }
  }

  for (DbgValueHistoryMap::iterator I = DbgValues.begin(), E = DbgValues.end();
       I != E; ++I) {
    SmallVectorImpl<const MachineInstr*> &History = I->second;
    if (History.empty())
      continue;

    // Make sure the final register assignments are terminated.
    const MachineInstr *Prev = History.back();
    if (Prev->isDebugValue() && isDbgValueInDefinedReg(Prev)) {
      const MachineBasicBlock *PrevMBB = Prev->getParent();
      MachineBasicBlock::const_iterator LastMI = 
        PrevMBB->getLastNonDebugInstr();
      if (LastMI == PrevMBB->end())
        // Drop DBG_VALUE for empty range.
        History.pop_back();
      else {
        // Terminate after LastMI.
        History.push_back(LastMI);
      }
    }
    // Request labels for the full history.
    for (unsigned i = 0, e = History.size(); i != e; ++i) {
      const MachineInstr *MI = History[i];
      if (MI->isDebugValue())
        requestLabelBeforeInsn(MI);
      else
        requestLabelAfterInsn(MI);
    }
  }

  PrevInstLoc = DebugLoc();
  PrevLabel = FunctionBeginSym;

  // Record beginning of function.
  if (!PrologEndLoc.isUnknown()) {
    DebugLoc FnStartDL = getFnDebugLoc(PrologEndLoc,
                                       MF->getFunction()->getContext());
    recordSourceLine(FnStartDL.getLine(), FnStartDL.getCol(),
                     FnStartDL.getScope(MF->getFunction()->getContext()),
                     DWARF2_FLAG_IS_STMT);
  }
}

void DwarfDebug::addScopeVariable(LexicalScope *LS, DbgVariable *Var) {
//  SmallVector<DbgVariable *, 8> &Vars = ScopeVariables.lookup(LS);
  ScopeVariables[LS].push_back(Var);
//  Vars.push_back(Var);
}

/// endFunction - Gather and emit post-function debug information.
///
void DwarfDebug::endFunction(const MachineFunction *MF) {
  if (!MMI->hasDebugInfo() || LScopes.empty()) return;

  // Define end label for subprogram.
  FunctionEndSym = Asm->GetTempSymbol("func_end",
                                      Asm->getFunctionNumber());
  // Assumes in correct section after the entry point.
  Asm->OutStreamer.EmitLabel(FunctionEndSym);
  
  SmallPtrSet<const MDNode *, 16> ProcessedVars;
  collectVariableInfo(MF, ProcessedVars);
  
  LexicalScope *FnScope = LScopes.getCurrentFunctionScope();
  CompileUnit *TheCU = SPMap.lookup(FnScope->getScopeNode());
  assert (TheCU && "Unable to find compile unit!");

  // Construct abstract scopes.
  ArrayRef<LexicalScope *> AList = LScopes.getAbstractScopesList();
  for (unsigned i = 0, e = AList.size(); i != e; ++i) {
    LexicalScope *AScope = AList[i];
    DISubprogram SP(AScope->getScopeNode());
    if (SP.Verify()) {
      // Collect info for variables that were optimized out.
      DIArray Variables = SP.getVariables();
      for (unsigned i = 0, e = Variables.getNumElements(); i != e; ++i) {
        DIVariable DV(Variables.getElement(i));
        if (!DV || !DV.Verify() || !ProcessedVars.insert(DV))
          continue;
        if (LexicalScope *Scope = LScopes.findAbstractScope(DV.getContext()))
          addScopeVariable(Scope, new DbgVariable(DV, NULL));
      }
    }
    if (ProcessedSPNodes.count(AScope->getScopeNode()) == 0)
      constructScopeDIE(TheCU, AScope);
  }
  
  DIE *CurFnDIE = constructScopeDIE(TheCU, FnScope);
  
  if (!DisableFramePointerElim(*MF))
    TheCU->addUInt(CurFnDIE, dwarf::DW_AT_APPLE_omit_frame_ptr,
                   dwarf::DW_FORM_flag, 1);

  DebugFrames.push_back(FunctionDebugFrameInfo(Asm->getFunctionNumber(),
                                               MMI->getFrameMoves()));

  // Clear debug info
  for (DenseMap<LexicalScope *, SmallVector<DbgVariable *, 8> >::iterator
         I = ScopeVariables.begin(), E = ScopeVariables.end(); I != E; ++I)
    DeleteContainerPointers(I->second);
  ScopeVariables.clear();
  DeleteContainerPointers(CurrentFnArguments);
  UserVariables.clear();
  DbgValues.clear();
  AbstractVariables.clear();
  LabelsBeforeInsn.clear();
  LabelsAfterInsn.clear();
  PrevLabel = NULL;
}

/// recordSourceLine - Register a source line with debug info. Returns the
/// unique label that was emitted and which provides correspondence to
/// the source line list.
void DwarfDebug::recordSourceLine(unsigned Line, unsigned Col, const MDNode *S,
                                  unsigned Flags) {
  StringRef Fn;
  StringRef Dir;
  unsigned Src = 1;
  if (S) {
    DIDescriptor Scope(S);

    if (Scope.isCompileUnit()) {
      DICompileUnit CU(S);
      Fn = CU.getFilename();
      Dir = CU.getDirectory();
    } else if (Scope.isFile()) {
      DIFile F(S);
      Fn = F.getFilename();
      Dir = F.getDirectory();
    } else if (Scope.isSubprogram()) {
      DISubprogram SP(S);
      Fn = SP.getFilename();
      Dir = SP.getDirectory();
    } else if (Scope.isLexicalBlockFile()) {
      DILexicalBlockFile DBF(S);
      Fn = DBF.getFilename();
      Dir = DBF.getDirectory();
    } else if (Scope.isLexicalBlock()) {
      DILexicalBlock DB(S);
      Fn = DB.getFilename();
      Dir = DB.getDirectory();
    } else
      assert(0 && "Unexpected scope info");

    Src = GetOrCreateSourceID(Fn, Dir);
  }
  Asm->OutStreamer.EmitDwarfLocDirective(Src, Line, Col, Flags, 0, 0, Fn);
}

//===----------------------------------------------------------------------===//
// Emit Methods
//===----------------------------------------------------------------------===//

/// computeSizeAndOffset - Compute the size and offset of a DIE.
///
unsigned
DwarfDebug::computeSizeAndOffset(DIE *Die, unsigned Offset, bool Last) {
  // Get the children.
  const std::vector<DIE *> &Children = Die->getChildren();

  // If not last sibling and has children then add sibling offset attribute.
  if (!Last && !Children.empty())
    Die->addSiblingOffset(DIEValueAllocator);

  // Record the abbreviation.
  assignAbbrevNumber(Die->getAbbrev());

  // Get the abbreviation for this DIE.
  unsigned AbbrevNumber = Die->getAbbrevNumber();
  const DIEAbbrev *Abbrev = Abbreviations[AbbrevNumber - 1];

  // Set DIE offset
  Die->setOffset(Offset);

  // Start the size with the size of abbreviation code.
  Offset += MCAsmInfo::getULEB128Size(AbbrevNumber);

  const SmallVector<DIEValue*, 32> &Values = Die->getValues();
  const SmallVector<DIEAbbrevData, 8> &AbbrevData = Abbrev->getData();

  // Size the DIE attribute values.
  for (unsigned i = 0, N = Values.size(); i < N; ++i)
    // Size attribute value.
    Offset += Values[i]->SizeOf(Asm, AbbrevData[i].getForm());

  // Size the DIE children if any.
  if (!Children.empty()) {
    assert(Abbrev->getChildrenFlag() == dwarf::DW_CHILDREN_yes &&
           "Children flag not set");

    for (unsigned j = 0, M = Children.size(); j < M; ++j)
      Offset = computeSizeAndOffset(Children[j], Offset, (j + 1) == M);

    // End of children marker.
    Offset += sizeof(int8_t);
  }

  Die->setSize(Offset - Die->getOffset());
  return Offset;
}

/// computeSizeAndOffsets - Compute the size and offset of all the DIEs.
///
void DwarfDebug::computeSizeAndOffsets() {
  for (DenseMap<const MDNode *, CompileUnit *>::iterator I = CUMap.begin(),
         E = CUMap.end(); I != E; ++I) {
    // Compute size of compile unit header.
    unsigned Offset = 
      sizeof(int32_t) + // Length of Compilation Unit Info
      sizeof(int16_t) + // DWARF version number
      sizeof(int32_t) + // Offset Into Abbrev. Section
      sizeof(int8_t);   // Pointer Size (in bytes)
    computeSizeAndOffset(I->second->getCUDie(), Offset, true);
  }
}

/// EmitSectionSym - Switch to the specified MCSection and emit an assembler
/// temporary label to it if SymbolStem is specified.
static MCSymbol *EmitSectionSym(AsmPrinter *Asm, const MCSection *Section,
                                const char *SymbolStem = 0) {
  Asm->OutStreamer.SwitchSection(Section);
  if (!SymbolStem) return 0;

  MCSymbol *TmpSym = Asm->GetTempSymbol(SymbolStem);
  Asm->OutStreamer.EmitLabel(TmpSym);
  return TmpSym;
}

/// EmitSectionLabels - Emit initial Dwarf sections with a label at
/// the start of each one.
void DwarfDebug::EmitSectionLabels() {
  const TargetLoweringObjectFile &TLOF = Asm->getObjFileLowering();

  // Dwarf sections base addresses.
  DwarfInfoSectionSym =
    EmitSectionSym(Asm, TLOF.getDwarfInfoSection(), "section_info");
  DwarfAbbrevSectionSym =
    EmitSectionSym(Asm, TLOF.getDwarfAbbrevSection(), "section_abbrev");
  EmitSectionSym(Asm, TLOF.getDwarfARangesSection());

  if (const MCSection *MacroInfo = TLOF.getDwarfMacroInfoSection())
    EmitSectionSym(Asm, MacroInfo);

  EmitSectionSym(Asm, TLOF.getDwarfLineSection(), "section_line");
  EmitSectionSym(Asm, TLOF.getDwarfLocSection());
  EmitSectionSym(Asm, TLOF.getDwarfPubNamesSection());
  EmitSectionSym(Asm, TLOF.getDwarfPubTypesSection());
  DwarfStrSectionSym =
    EmitSectionSym(Asm, TLOF.getDwarfStrSection(), "section_str");
  DwarfDebugRangeSectionSym = EmitSectionSym(Asm, TLOF.getDwarfRangesSection(),
                                             "debug_range");

  DwarfDebugLocSectionSym = EmitSectionSym(Asm, TLOF.getDwarfLocSection(),
                                           "section_debug_loc");

  TextSectionSym = EmitSectionSym(Asm, TLOF.getTextSection(), "text_begin");
  EmitSectionSym(Asm, TLOF.getDataSection());
}

/// emitDIE - Recursively emits a debug information entry.
///
void DwarfDebug::emitDIE(DIE *Die) {
  // Get the abbreviation for this DIE.
  unsigned AbbrevNumber = Die->getAbbrevNumber();
  const DIEAbbrev *Abbrev = Abbreviations[AbbrevNumber - 1];

  // Emit the code (index) for the abbreviation.
  if (Asm->isVerbose())
    Asm->OutStreamer.AddComment("Abbrev [" + Twine(AbbrevNumber) + "] 0x" +
                                Twine::utohexstr(Die->getOffset()) + ":0x" +
                                Twine::utohexstr(Die->getSize()) + " " +
                                dwarf::TagString(Abbrev->getTag()));
  Asm->EmitULEB128(AbbrevNumber);

  const SmallVector<DIEValue*, 32> &Values = Die->getValues();
  const SmallVector<DIEAbbrevData, 8> &AbbrevData = Abbrev->getData();

  // Emit the DIE attribute values.
  for (unsigned i = 0, N = Values.size(); i < N; ++i) {
    unsigned Attr = AbbrevData[i].getAttribute();
    unsigned Form = AbbrevData[i].getForm();
    assert(Form && "Too many attributes for DIE (check abbreviation)");

    if (Asm->isVerbose())
      Asm->OutStreamer.AddComment(dwarf::AttributeString(Attr));

    switch (Attr) {
    case dwarf::DW_AT_sibling:
      Asm->EmitInt32(Die->getSiblingOffset());
      break;
    case dwarf::DW_AT_abstract_origin: {
      DIEEntry *E = cast<DIEEntry>(Values[i]);
      DIE *Origin = E->getEntry();
      unsigned Addr = Origin->getOffset();
      Asm->EmitInt32(Addr);
      break;
    }
    case dwarf::DW_AT_ranges: {
      // DW_AT_range Value encodes offset in debug_range section.
      DIEInteger *V = cast<DIEInteger>(Values[i]);

      if (Asm->MAI->doesDwarfUsesLabelOffsetForRanges()) {
        Asm->EmitLabelPlusOffset(DwarfDebugRangeSectionSym,
                                 V->getValue(),
                                 4);
      } else {
        Asm->EmitLabelOffsetDifference(DwarfDebugRangeSectionSym,
                                       V->getValue(),
                                       DwarfDebugRangeSectionSym,
                                       4);
      }
      break;
    }
    case dwarf::DW_AT_location: {
      if (DIELabel *L = dyn_cast<DIELabel>(Values[i]))
        Asm->EmitLabelDifference(L->getValue(), DwarfDebugLocSectionSym, 4);
      else
        Values[i]->EmitValue(Asm, Form);
      break;
    }
    case dwarf::DW_AT_accessibility: {
      if (Asm->isVerbose()) {
        DIEInteger *V = cast<DIEInteger>(Values[i]);
        Asm->OutStreamer.AddComment(dwarf::AccessibilityString(V->getValue()));
      }
      Values[i]->EmitValue(Asm, Form);
      break;
    }
    default:
      // Emit an attribute using the defined form.
      Values[i]->EmitValue(Asm, Form);
      break;
    }
  }

  // Emit the DIE children if any.
  if (Abbrev->getChildrenFlag() == dwarf::DW_CHILDREN_yes) {
    const std::vector<DIE *> &Children = Die->getChildren();

    for (unsigned j = 0, M = Children.size(); j < M; ++j)
      emitDIE(Children[j]);

    if (Asm->isVerbose())
      Asm->OutStreamer.AddComment("End Of Children Mark");
    Asm->EmitInt8(0);
  }
}

/// emitDebugInfo - Emit the debug info section.
///
void DwarfDebug::emitDebugInfo() {
  // Start debug info section.
  Asm->OutStreamer.SwitchSection(
                            Asm->getObjFileLowering().getDwarfInfoSection());
  for (DenseMap<const MDNode *, CompileUnit *>::iterator I = CUMap.begin(),
         E = CUMap.end(); I != E; ++I) {
    CompileUnit *TheCU = I->second;
    DIE *Die = TheCU->getCUDie();

    // Emit the compile units header.
    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("info_begin",
                                                  TheCU->getID()));

    // Emit size of content not including length itself
    unsigned ContentSize = Die->getSize() +
      sizeof(int16_t) + // DWARF version number
      sizeof(int32_t) + // Offset Into Abbrev. Section
      sizeof(int8_t);   // Pointer Size (in bytes)

    Asm->OutStreamer.AddComment("Length of Compilation Unit Info");
    Asm->EmitInt32(ContentSize);
    Asm->OutStreamer.AddComment("DWARF version number");
    Asm->EmitInt16(dwarf::DWARF_VERSION);
    Asm->OutStreamer.AddComment("Offset Into Abbrev. Section");
    Asm->EmitSectionOffset(Asm->GetTempSymbol("abbrev_begin"),
                           DwarfAbbrevSectionSym);
    Asm->OutStreamer.AddComment("Address Size (in bytes)");
    Asm->EmitInt8(Asm->getTargetData().getPointerSize());

    emitDIE(Die);
    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("info_end", TheCU->getID()));
  }
}

/// emitAbbreviations - Emit the abbreviation section.
///
void DwarfDebug::emitAbbreviations() const {
  // Check to see if it is worth the effort.
  if (!Abbreviations.empty()) {
    // Start the debug abbrev section.
    Asm->OutStreamer.SwitchSection(
                            Asm->getObjFileLowering().getDwarfAbbrevSection());

    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("abbrev_begin"));

    // For each abbrevation.
    for (unsigned i = 0, N = Abbreviations.size(); i < N; ++i) {
      // Get abbreviation data
      const DIEAbbrev *Abbrev = Abbreviations[i];

      // Emit the abbrevations code (base 1 index.)
      Asm->EmitULEB128(Abbrev->getNumber(), "Abbreviation Code");

      // Emit the abbreviations data.
      Abbrev->Emit(Asm);
    }

    // Mark end of abbreviations.
    Asm->EmitULEB128(0, "EOM(3)");

    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("abbrev_end"));
  }
}

/// emitEndOfLineMatrix - Emit the last address of the section and the end of
/// the line matrix.
///
void DwarfDebug::emitEndOfLineMatrix(unsigned SectionEnd) {
  // Define last address of section.
  Asm->OutStreamer.AddComment("Extended Op");
  Asm->EmitInt8(0);

  Asm->OutStreamer.AddComment("Op size");
  Asm->EmitInt8(Asm->getTargetData().getPointerSize() + 1);
  Asm->OutStreamer.AddComment("DW_LNE_set_address");
  Asm->EmitInt8(dwarf::DW_LNE_set_address);

  Asm->OutStreamer.AddComment("Section end label");

  Asm->OutStreamer.EmitSymbolValue(Asm->GetTempSymbol("section_end",SectionEnd),
                                   Asm->getTargetData().getPointerSize(),
                                   0/*AddrSpace*/);

  // Mark end of matrix.
  Asm->OutStreamer.AddComment("DW_LNE_end_sequence");
  Asm->EmitInt8(0);
  Asm->EmitInt8(1);
  Asm->EmitInt8(1);
}

/// emitDebugPubNames - Emit visible names into a debug pubnames section.
///
void DwarfDebug::emitDebugPubNames() {
  for (DenseMap<const MDNode *, CompileUnit *>::iterator I = CUMap.begin(),
         E = CUMap.end(); I != E; ++I) {
    CompileUnit *TheCU = I->second;
    // Start the dwarf pubnames section.
    Asm->OutStreamer.SwitchSection(
      Asm->getObjFileLowering().getDwarfPubNamesSection());

    Asm->OutStreamer.AddComment("Length of Public Names Info");
    Asm->EmitLabelDifference(
      Asm->GetTempSymbol("pubnames_end", TheCU->getID()),
      Asm->GetTempSymbol("pubnames_begin", TheCU->getID()), 4);

    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("pubnames_begin",
                                                  TheCU->getID()));

    Asm->OutStreamer.AddComment("DWARF Version");
    Asm->EmitInt16(dwarf::DWARF_VERSION);

    Asm->OutStreamer.AddComment("Offset of Compilation Unit Info");
    Asm->EmitSectionOffset(Asm->GetTempSymbol("info_begin", TheCU->getID()),
                           DwarfInfoSectionSym);

    Asm->OutStreamer.AddComment("Compilation Unit Length");
    Asm->EmitLabelDifference(Asm->GetTempSymbol("info_end", TheCU->getID()),
                             Asm->GetTempSymbol("info_begin", TheCU->getID()),
                             4);

    const StringMap<DIE*> &Globals = TheCU->getGlobals();
    for (StringMap<DIE*>::const_iterator
           GI = Globals.begin(), GE = Globals.end(); GI != GE; ++GI) {
      const char *Name = GI->getKeyData();
      DIE *Entity = GI->second;

      Asm->OutStreamer.AddComment("DIE offset");
      Asm->EmitInt32(Entity->getOffset());

      if (Asm->isVerbose())
        Asm->OutStreamer.AddComment("External Name");
      Asm->OutStreamer.EmitBytes(StringRef(Name, strlen(Name)+1), 0);
    }

    Asm->OutStreamer.AddComment("End Mark");
    Asm->EmitInt32(0);
    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("pubnames_end",
                                                  TheCU->getID()));
  }
}

void DwarfDebug::emitDebugPubTypes() {
  for (DenseMap<const MDNode *, CompileUnit *>::iterator I = CUMap.begin(),
         E = CUMap.end(); I != E; ++I) {
    CompileUnit *TheCU = I->second;
    // Start the dwarf pubnames section.
    Asm->OutStreamer.SwitchSection(
      Asm->getObjFileLowering().getDwarfPubTypesSection());
    Asm->OutStreamer.AddComment("Length of Public Types Info");
    Asm->EmitLabelDifference(
      Asm->GetTempSymbol("pubtypes_end", TheCU->getID()),
      Asm->GetTempSymbol("pubtypes_begin", TheCU->getID()), 4);

    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("pubtypes_begin",
                                                  TheCU->getID()));

    if (Asm->isVerbose()) Asm->OutStreamer.AddComment("DWARF Version");
    Asm->EmitInt16(dwarf::DWARF_VERSION);

    Asm->OutStreamer.AddComment("Offset of Compilation Unit Info");
    Asm->EmitSectionOffset(Asm->GetTempSymbol("info_begin", TheCU->getID()),
                           DwarfInfoSectionSym);

    Asm->OutStreamer.AddComment("Compilation Unit Length");
    Asm->EmitLabelDifference(Asm->GetTempSymbol("info_end", TheCU->getID()),
                             Asm->GetTempSymbol("info_begin", TheCU->getID()),
                             4);

    const StringMap<DIE*> &Globals = TheCU->getGlobalTypes();
    for (StringMap<DIE*>::const_iterator
           GI = Globals.begin(), GE = Globals.end(); GI != GE; ++GI) {
      const char *Name = GI->getKeyData();
      DIE *Entity = GI->second;

      if (Asm->isVerbose()) Asm->OutStreamer.AddComment("DIE offset");
      Asm->EmitInt32(Entity->getOffset());

      if (Asm->isVerbose()) Asm->OutStreamer.AddComment("External Name");
      Asm->OutStreamer.EmitBytes(StringRef(Name, GI->getKeyLength()+1), 0);
    }

    Asm->OutStreamer.AddComment("End Mark");
    Asm->EmitInt32(0);
    Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("pubtypes_end",
                                                  TheCU->getID()));
  }
}

/// emitDebugStr - Emit visible names into a debug str section.
///
void DwarfDebug::emitDebugStr() {
  // Check to see if it is worth the effort.
  if (StringPool.empty()) return;

  // Start the dwarf str section.
  Asm->OutStreamer.SwitchSection(
                                Asm->getObjFileLowering().getDwarfStrSection());

  // Get all of the string pool entries and put them in an array by their ID so
  // we can sort them.
  SmallVector<std::pair<unsigned,
      StringMapEntry<std::pair<MCSymbol*, unsigned> >*>, 64> Entries;

  for (StringMap<std::pair<MCSymbol*, unsigned> >::iterator
       I = StringPool.begin(), E = StringPool.end(); I != E; ++I)
    Entries.push_back(std::make_pair(I->second.second, &*I));

  array_pod_sort(Entries.begin(), Entries.end());

  for (unsigned i = 0, e = Entries.size(); i != e; ++i) {
    // Emit a label for reference from debug information entries.
    Asm->OutStreamer.EmitLabel(Entries[i].second->getValue().first);

    // Emit the string itself.
    Asm->OutStreamer.EmitBytes(Entries[i].second->getKey(), 0/*addrspace*/);
  }
}

/// emitDebugLoc - Emit visible names into a debug loc section.
///
void DwarfDebug::emitDebugLoc() {
  if (DotDebugLocEntries.empty())
    return;

  for (SmallVector<DotDebugLocEntry, 4>::iterator
         I = DotDebugLocEntries.begin(), E = DotDebugLocEntries.end();
       I != E; ++I) {
    DotDebugLocEntry &Entry = *I;
    if (I + 1 != DotDebugLocEntries.end())
      Entry.Merge(I+1);
  }

  // Start the dwarf loc section.
  Asm->OutStreamer.SwitchSection(
    Asm->getObjFileLowering().getDwarfLocSection());
  unsigned char Size = Asm->getTargetData().getPointerSize();
  Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("debug_loc", 0));
  unsigned index = 1;
  for (SmallVector<DotDebugLocEntry, 4>::iterator
         I = DotDebugLocEntries.begin(), E = DotDebugLocEntries.end();
       I != E; ++I, ++index) {
    DotDebugLocEntry &Entry = *I;
    if (Entry.isMerged()) continue;
    if (Entry.isEmpty()) {
      Asm->OutStreamer.EmitIntValue(0, Size, /*addrspace*/0);
      Asm->OutStreamer.EmitIntValue(0, Size, /*addrspace*/0);
      Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("debug_loc", index));
    } else {
      Asm->OutStreamer.EmitSymbolValue(Entry.Begin, Size, 0);
      Asm->OutStreamer.EmitSymbolValue(Entry.End, Size, 0);
      DIVariable DV(Entry.Variable);
      Asm->OutStreamer.AddComment("Loc expr size");
      MCSymbol *begin = Asm->OutStreamer.getContext().CreateTempSymbol();
      MCSymbol *end = Asm->OutStreamer.getContext().CreateTempSymbol();
      Asm->EmitLabelDifference(end, begin, 2);
      Asm->OutStreamer.EmitLabel(begin);
      if (Entry.isInt()) {
        DIBasicType BTy(DV.getType());
        if (BTy.Verify() &&
            (BTy.getEncoding()  == dwarf::DW_ATE_signed 
             || BTy.getEncoding() == dwarf::DW_ATE_signed_char)) {
          Asm->OutStreamer.AddComment("DW_OP_consts");
          Asm->EmitInt8(dwarf::DW_OP_consts);
          Asm->EmitSLEB128(Entry.getInt());
        } else {
          Asm->OutStreamer.AddComment("DW_OP_constu");
          Asm->EmitInt8(dwarf::DW_OP_constu);
          Asm->EmitULEB128(Entry.getInt());
        }
      } else if (Entry.isLocation()) {
        if (!DV.hasComplexAddress()) 
          // Regular entry.
          Asm->EmitDwarfRegOp(Entry.Loc);
        else {
          // Complex address entry.
          unsigned N = DV.getNumAddrElements();
          unsigned i = 0;
          if (N >= 2 && DV.getAddrElement(0) == DIBuilder::OpPlus) {
            if (Entry.Loc.getOffset()) {
              i = 2;
              Asm->EmitDwarfRegOp(Entry.Loc);
              Asm->OutStreamer.AddComment("DW_OP_deref");
              Asm->EmitInt8(dwarf::DW_OP_deref);
              Asm->OutStreamer.AddComment("DW_OP_plus_uconst");
              Asm->EmitInt8(dwarf::DW_OP_plus_uconst);
              Asm->EmitSLEB128(DV.getAddrElement(1));
            } else {
              // If first address element is OpPlus then emit
              // DW_OP_breg + Offset instead of DW_OP_reg + Offset.
              MachineLocation Loc(Entry.Loc.getReg(), DV.getAddrElement(1));
              Asm->EmitDwarfRegOp(Loc);
              i = 2;
            }
          } else {
            Asm->EmitDwarfRegOp(Entry.Loc);
          }
          
          // Emit remaining complex address elements.
          for (; i < N; ++i) {
            uint64_t Element = DV.getAddrElement(i);
            if (Element == DIBuilder::OpPlus) {
              Asm->EmitInt8(dwarf::DW_OP_plus_uconst);
              Asm->EmitULEB128(DV.getAddrElement(++i));
            } else if (Element == DIBuilder::OpDeref)
              Asm->EmitInt8(dwarf::DW_OP_deref);
            else llvm_unreachable("unknown Opcode found in complex address");
          }
        }
      }
      // else ... ignore constant fp. There is not any good way to
      // to represent them here in dwarf.
      Asm->OutStreamer.EmitLabel(end);
    }
  }
}

/// EmitDebugARanges - Emit visible names into a debug aranges section.
///
void DwarfDebug::EmitDebugARanges() {
  // Start the dwarf aranges section.
  Asm->OutStreamer.SwitchSection(
                          Asm->getObjFileLowering().getDwarfARangesSection());
}

/// emitDebugRanges - Emit visible names into a debug ranges section.
///
void DwarfDebug::emitDebugRanges() {
  // Start the dwarf ranges section.
  Asm->OutStreamer.SwitchSection(
    Asm->getObjFileLowering().getDwarfRangesSection());
  unsigned char Size = Asm->getTargetData().getPointerSize();
  for (SmallVector<const MCSymbol *, 8>::iterator
         I = DebugRangeSymbols.begin(), E = DebugRangeSymbols.end();
       I != E; ++I) {
    if (*I)
      Asm->OutStreamer.EmitSymbolValue(const_cast<MCSymbol*>(*I), Size, 0);
    else
      Asm->OutStreamer.EmitIntValue(0, Size, /*addrspace*/0);
  }
}

/// emitDebugMacInfo - Emit visible names into a debug macinfo section.
///
void DwarfDebug::emitDebugMacInfo() {
  if (const MCSection *LineInfo =
      Asm->getObjFileLowering().getDwarfMacroInfoSection()) {
    // Start the dwarf macinfo section.
    Asm->OutStreamer.SwitchSection(LineInfo);
  }
}

/// emitDebugInlineInfo - Emit inline info using following format.
/// Section Header:
/// 1. length of section
/// 2. Dwarf version number
/// 3. address size.
///
/// Entries (one "entry" for each function that was inlined):
///
/// 1. offset into __debug_str section for MIPS linkage name, if exists;
///   otherwise offset into __debug_str for regular function name.
/// 2. offset into __debug_str section for regular function name.
/// 3. an unsigned LEB128 number indicating the number of distinct inlining
/// instances for the function.
///
/// The rest of the entry consists of a {die_offset, low_pc} pair for each
/// inlined instance; the die_offset points to the inlined_subroutine die in the
/// __debug_info section, and the low_pc is the starting address for the
/// inlining instance.
void DwarfDebug::emitDebugInlineInfo() {
  if (!Asm->MAI->doesDwarfUsesInlineInfoSection())
    return;

  if (!FirstCU)
    return;

  Asm->OutStreamer.SwitchSection(
                        Asm->getObjFileLowering().getDwarfDebugInlineSection());

  Asm->OutStreamer.AddComment("Length of Debug Inlined Information Entry");
  Asm->EmitLabelDifference(Asm->GetTempSymbol("debug_inlined_end", 1),
                           Asm->GetTempSymbol("debug_inlined_begin", 1), 4);

  Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("debug_inlined_begin", 1));

  Asm->OutStreamer.AddComment("Dwarf Version");
  Asm->EmitInt16(dwarf::DWARF_VERSION);
  Asm->OutStreamer.AddComment("Address Size (in bytes)");
  Asm->EmitInt8(Asm->getTargetData().getPointerSize());

  for (SmallVector<const MDNode *, 4>::iterator I = InlinedSPNodes.begin(),
         E = InlinedSPNodes.end(); I != E; ++I) {

    const MDNode *Node = *I;
    DenseMap<const MDNode *, SmallVector<InlineInfoLabels, 4> >::iterator II
      = InlineInfo.find(Node);
    SmallVector<InlineInfoLabels, 4> &Labels = II->second;
    DISubprogram SP(Node);
    StringRef LName = SP.getLinkageName();
    StringRef Name = SP.getName();

    Asm->OutStreamer.AddComment("MIPS linkage name");
    if (LName.empty()) {
      Asm->OutStreamer.EmitBytes(Name, 0);
      Asm->OutStreamer.EmitIntValue(0, 1, 0); // nul terminator.
    } else
      Asm->EmitSectionOffset(getStringPoolEntry(getRealLinkageName(LName)),
                             DwarfStrSectionSym);

    Asm->OutStreamer.AddComment("Function name");
    Asm->EmitSectionOffset(getStringPoolEntry(Name), DwarfStrSectionSym);
    Asm->EmitULEB128(Labels.size(), "Inline count");

    for (SmallVector<InlineInfoLabels, 4>::iterator LI = Labels.begin(),
           LE = Labels.end(); LI != LE; ++LI) {
      if (Asm->isVerbose()) Asm->OutStreamer.AddComment("DIE offset");
      Asm->EmitInt32(LI->second->getOffset());

      if (Asm->isVerbose()) Asm->OutStreamer.AddComment("low_pc");
      Asm->OutStreamer.EmitSymbolValue(LI->first,
                                       Asm->getTargetData().getPointerSize(),0);
    }
  }

  Asm->OutStreamer.EmitLabel(Asm->GetTempSymbol("debug_inlined_end", 1));
}
