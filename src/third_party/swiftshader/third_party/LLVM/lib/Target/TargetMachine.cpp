//===-- TargetMachine.cpp - General Target Information ---------------------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the general parts of a Target machine.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;

//---------------------------------------------------------------------------
// Command-line options that tend to be useful on more than one back-end.
//

namespace llvm {
  bool LessPreciseFPMADOption;
  bool PrintMachineCode;
  bool NoFramePointerElim;
  bool NoFramePointerElimNonLeaf;
  bool NoExcessFPPrecision;
  bool UnsafeFPMath;
  bool NoInfsFPMath;
  bool NoNaNsFPMath;
  bool HonorSignDependentRoundingFPMathOption;
  bool UseSoftFloat;
  FloatABI::ABIType FloatABIType;
  bool NoImplicitFloat;
  bool NoZerosInBSS;
  bool JITExceptionHandling;
  bool JITEmitDebugInfo;
  bool JITEmitDebugInfoToDisk;
  bool GuaranteedTailCallOpt;
  unsigned StackAlignmentOverride;
  bool RealignStack;
  bool DisableJumpTables;
  bool StrongPHIElim;
  bool HasDivModLibcall;
  bool AsmVerbosityDefault(false);
  bool EnableSegmentedStacks;
}

static cl::opt<bool, true>
PrintCode("print-machineinstrs",
  cl::desc("Print generated machine code"),
  cl::location(PrintMachineCode), cl::init(false));
static cl::opt<bool, true>
DisableFPElim("disable-fp-elim",
  cl::desc("Disable frame pointer elimination optimization"),
  cl::location(NoFramePointerElim),
  cl::init(false));
static cl::opt<bool, true>
DisableFPElimNonLeaf("disable-non-leaf-fp-elim",
  cl::desc("Disable frame pointer elimination optimization for non-leaf funcs"),
  cl::location(NoFramePointerElimNonLeaf),
  cl::init(false));
static cl::opt<bool, true>
DisableExcessPrecision("disable-excess-fp-precision",
  cl::desc("Disable optimizations that may increase FP precision"),
  cl::location(NoExcessFPPrecision),
  cl::init(false));
static cl::opt<bool, true>
EnableFPMAD("enable-fp-mad",
  cl::desc("Enable less precise MAD instructions to be generated"),
  cl::location(LessPreciseFPMADOption),
  cl::init(false));
static cl::opt<bool, true>
EnableUnsafeFPMath("enable-unsafe-fp-math",
  cl::desc("Enable optimizations that may decrease FP precision"),
  cl::location(UnsafeFPMath),
  cl::init(false));
static cl::opt<bool, true>
EnableNoInfsFPMath("enable-no-infs-fp-math",
  cl::desc("Enable FP math optimizations that assume no +-Infs"),
  cl::location(NoInfsFPMath),
  cl::init(false));
static cl::opt<bool, true>
EnableNoNaNsFPMath("enable-no-nans-fp-math",
  cl::desc("Enable FP math optimizations that assume no NaNs"),
  cl::location(NoNaNsFPMath),
  cl::init(false));
static cl::opt<bool, true>
EnableHonorSignDependentRoundingFPMath("enable-sign-dependent-rounding-fp-math",
  cl::Hidden,
  cl::desc("Force codegen to assume rounding mode can change dynamically"),
  cl::location(HonorSignDependentRoundingFPMathOption),
  cl::init(false));
static cl::opt<bool, true>
GenerateSoftFloatCalls("soft-float",
  cl::desc("Generate software floating point library calls"),
  cl::location(UseSoftFloat),
  cl::init(false));
static cl::opt<llvm::FloatABI::ABIType, true>
FloatABIForCalls("float-abi",
  cl::desc("Choose float ABI type"),
  cl::location(FloatABIType),
  cl::init(FloatABI::Default),
  cl::values(
    clEnumValN(FloatABI::Default, "default",
               "Target default float ABI type"),
    clEnumValN(FloatABI::Soft, "soft",
               "Soft float ABI (implied by -soft-float)"),
    clEnumValN(FloatABI::Hard, "hard",
               "Hard float ABI (uses FP registers)"),
    clEnumValEnd));
static cl::opt<bool, true>
DontPlaceZerosInBSS("nozero-initialized-in-bss",
  cl::desc("Don't place zero-initialized symbols into bss section"),
  cl::location(NoZerosInBSS),
  cl::init(false));
static cl::opt<bool, true>
EnableJITExceptionHandling("jit-enable-eh",
  cl::desc("Emit exception handling information"),
  cl::location(JITExceptionHandling),
  cl::init(false));
// In debug builds, make this default to true.
#ifdef NDEBUG
#define EMIT_DEBUG false
#else
#define EMIT_DEBUG true
#endif
static cl::opt<bool, true>
EmitJitDebugInfo("jit-emit-debug",
  cl::desc("Emit debug information to debugger"),
  cl::location(JITEmitDebugInfo),
  cl::init(EMIT_DEBUG));
#undef EMIT_DEBUG
static cl::opt<bool, true>
EmitJitDebugInfoToDisk("jit-emit-debug-to-disk",
  cl::Hidden,
  cl::desc("Emit debug info objfiles to disk"),
  cl::location(JITEmitDebugInfoToDisk),
  cl::init(false));

static cl::opt<bool, true>
EnableGuaranteedTailCallOpt("tailcallopt",
  cl::desc("Turn fastcc calls into tail calls by (potentially) changing ABI."),
  cl::location(GuaranteedTailCallOpt),
  cl::init(false));
static cl::opt<unsigned, true>
OverrideStackAlignment("stack-alignment",
  cl::desc("Override default stack alignment"),
  cl::location(StackAlignmentOverride),
  cl::init(0));
static cl::opt<bool, true>
EnableRealignStack("realign-stack",
  cl::desc("Realign stack if needed"),
  cl::location(RealignStack),
  cl::init(true));
static cl::opt<bool, true>
DisableSwitchTables(cl::Hidden, "disable-jump-tables", 
  cl::desc("Do not generate jump tables."),
  cl::location(DisableJumpTables),
  cl::init(false));
static cl::opt<bool, true>
EnableStrongPHIElim(cl::Hidden, "strong-phi-elim",
  cl::desc("Use strong PHI elimination."),
  cl::location(StrongPHIElim),
  cl::init(false));
static cl::opt<std::string>
TrapFuncName("trap-func", cl::Hidden,
  cl::desc("Emit a call to trap function rather than a trap instruction"),
  cl::init(""));
static cl::opt<bool>
DataSections("fdata-sections",
  cl::desc("Emit data into separate sections"),
  cl::init(false));
static cl::opt<bool>
FunctionSections("ffunction-sections",
  cl::desc("Emit functions into separate sections"),
  cl::init(false));
static cl::opt<bool, true>
SegmentedStacks("segmented-stacks",
  cl::desc("Use segmented stacks if possible."),
  cl::location(EnableSegmentedStacks),
  cl::init(false));
                         
//---------------------------------------------------------------------------
// TargetMachine Class
//

TargetMachine::TargetMachine(const Target &T,
                             StringRef TT, StringRef CPU, StringRef FS)
  : TheTarget(T), TargetTriple(TT), TargetCPU(CPU), TargetFS(FS),
    CodeGenInfo(0), AsmInfo(0),
    MCRelaxAll(false),
    MCNoExecStack(false),
    MCSaveTempLabels(false),
    MCUseLoc(true),
    MCUseCFI(true) {
  // Typically it will be subtargets that will adjust FloatABIType from Default
  // to Soft or Hard.
  if (UseSoftFloat)
    FloatABIType = FloatABI::Soft;
}

TargetMachine::~TargetMachine() {
  delete CodeGenInfo;
  delete AsmInfo;
}

/// getRelocationModel - Returns the code generation relocation model. The
/// choices are static, PIC, and dynamic-no-pic, and target default.
Reloc::Model TargetMachine::getRelocationModel() const {
  if (!CodeGenInfo)
    return Reloc::Default;
  return CodeGenInfo->getRelocationModel();
}

/// getCodeModel - Returns the code model. The choices are small, kernel,
/// medium, large, and target default.
CodeModel::Model TargetMachine::getCodeModel() const {
  if (!CodeGenInfo)
    return CodeModel::Default;
  return CodeGenInfo->getCodeModel();
}

bool TargetMachine::getAsmVerbosityDefault() {
  return AsmVerbosityDefault;
}

void TargetMachine::setAsmVerbosityDefault(bool V) {
  AsmVerbosityDefault = V;
}

bool TargetMachine::getFunctionSections() {
  return FunctionSections;
}

bool TargetMachine::getDataSections() {
  return DataSections;
}

void TargetMachine::setFunctionSections(bool V) {
  FunctionSections = V;
}

void TargetMachine::setDataSections(bool V) {
  DataSections = V;
}

namespace llvm {
  /// DisableFramePointerElim - This returns true if frame pointer elimination
  /// optimization should be disabled for the given machine function.
  bool DisableFramePointerElim(const MachineFunction &MF) {
    // Check to see if we should eliminate non-leaf frame pointers and then
    // check to see if we should eliminate all frame pointers.
    if (NoFramePointerElimNonLeaf && !NoFramePointerElim) {
      const MachineFrameInfo *MFI = MF.getFrameInfo();
      return MFI->hasCalls();
    }

    return NoFramePointerElim;
  }

  /// LessPreciseFPMAD - This flag return true when -enable-fp-mad option
  /// is specified on the command line.  When this flag is off(default), the
  /// code generator is not allowed to generate mad (multiply add) if the
  /// result is "less precise" than doing those operations individually.
  bool LessPreciseFPMAD() { return UnsafeFPMath || LessPreciseFPMADOption; }

  /// HonorSignDependentRoundingFPMath - Return true if the codegen must assume
  /// that the rounding mode of the FPU can change from its default.
  bool HonorSignDependentRoundingFPMath() {
    return !UnsafeFPMath && HonorSignDependentRoundingFPMathOption;
  }

  /// getTrapFunctionName - If this returns a non-empty string, this means isel
  /// should lower Intrinsic::trap to a call to the specified function name
  /// instead of an ISD::TRAP node.
  StringRef getTrapFunctionName() {
    return TrapFuncName;
  }
}
