LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CLANG := true

LOCAL_MODULE := libLLVM_swiftshader
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES := \
	lib/Analysis/AliasAnalysis.cpp \
	lib/Analysis/AliasSetTracker.cpp \
	lib/Analysis/BasicAliasAnalysis.cpp \
	lib/Analysis/BranchProbabilityInfo.cpp \
	lib/Analysis/CaptureTracking.cpp \
	lib/Analysis/ConstantFolding.cpp \
	lib/Analysis/DebugInfo.cpp \
	lib/Analysis/DIBuilder.cpp \
	lib/Analysis/InstructionSimplify.cpp \
	lib/Analysis/IVUsers.cpp \
	lib/Analysis/Loads.cpp \
	lib/Analysis/LoopInfo.cpp \
	lib/Analysis/LoopPass.cpp \
	lib/Analysis/MemoryBuiltins.cpp \
	lib/Analysis/MemoryDependenceAnalysis.cpp \
	lib/Analysis/NoAliasAnalysis.cpp \
	lib/Analysis/PathNumbering.cpp \
	lib/Analysis/PHITransAddr.cpp \
	lib/Analysis/ProfileInfo.cpp \
	lib/Analysis/ScalarEvolution.cpp \
	lib/Analysis/ScalarEvolutionExpander.cpp \
	lib/Analysis/ScalarEvolutionNormalization.cpp \
	lib/Analysis/TypeBasedAliasAnalysis.cpp \
	lib/Analysis/ValueTracking.cpp \

LOCAL_SRC_FILES += \
	lib/CodeGen/SelectionDAG/DAGCombiner.cpp \
	lib/CodeGen/SelectionDAG/FastISel.cpp \
	lib/CodeGen/SelectionDAG/FunctionLoweringInfo.cpp \
	lib/CodeGen/SelectionDAG/InstrEmitter.cpp \
	lib/CodeGen/SelectionDAG/LegalizeDAG.cpp \
	lib/CodeGen/SelectionDAG/LegalizeFloatTypes.cpp \
	lib/CodeGen/SelectionDAG/LegalizeIntegerTypes.cpp \
	lib/CodeGen/SelectionDAG/LegalizeTypes.cpp \
	lib/CodeGen/SelectionDAG/LegalizeTypesGeneric.cpp \
	lib/CodeGen/SelectionDAG/LegalizeVectorOps.cpp \
	lib/CodeGen/SelectionDAG/LegalizeVectorTypes.cpp \
	lib/CodeGen/SelectionDAG/ScheduleDAGFast.cpp \
	lib/CodeGen/SelectionDAG/ScheduleDAGList.cpp \
	lib/CodeGen/SelectionDAG/ScheduleDAGRRList.cpp \
	lib/CodeGen/SelectionDAG/ScheduleDAGSDNodes.cpp \
	lib/CodeGen/SelectionDAG/SelectionDAG.cpp \
	lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp \
	lib/CodeGen/SelectionDAG/SelectionDAGISel.cpp \
	lib/CodeGen/SelectionDAG/SelectionDAGPrinter.cpp \
	lib/CodeGen/SelectionDAG/TargetLowering.cpp \
	lib/CodeGen/SelectionDAG/TargetSelectionDAGInfo.cpp \
	lib/CodeGen/AggressiveAntiDepBreaker.cpp \
	lib/CodeGen/AllocationOrder.cpp \
	lib/CodeGen/Analysis.cpp \
	lib/CodeGen/BranchFolding.cpp \
	lib/CodeGen/CalcSpillWeights.cpp \
	lib/CodeGen/CallingConvLower.cpp \
	lib/CodeGen/CodeGen.cpp \
	lib/CodeGen/CodePlacementOpt.cpp \
	lib/CodeGen/CriticalAntiDepBreaker.cpp \
	lib/CodeGen/DeadMachineInstructionElim.cpp \
	lib/CodeGen/DwarfEHPrepare.cpp \
	lib/CodeGen/EdgeBundles.cpp \
	lib/CodeGen/ELFCodeEmitter.cpp \
	lib/CodeGen/ELFWriter.cpp \
	lib/CodeGen/ExecutionDepsFix.cpp \
	lib/CodeGen/ExpandISelPseudos.cpp \
	lib/CodeGen/ExpandPostRAPseudos.cpp \
	lib/CodeGen/GCMetadata.cpp \
	lib/CodeGen/GCStrategy.cpp \
	lib/CodeGen/IfConversion.cpp \
	lib/CodeGen/InlineSpiller.cpp \
	lib/CodeGen/InterferenceCache.cpp \
	lib/CodeGen/IntrinsicLowering.cpp \
	lib/CodeGen/LatencyPriorityQueue.cpp \
	lib/CodeGen/LexicalScopes.cpp \
	lib/CodeGen/LiveDebugVariables.cpp \
	lib/CodeGen/LiveIntervalAnalysis.cpp \
	lib/CodeGen/LiveInterval.cpp \
	lib/CodeGen/LiveIntervalUnion.cpp \
	lib/CodeGen/LiveRangeCalc.cpp \
	lib/CodeGen/LiveRangeEdit.cpp \
	lib/CodeGen/LiveStackAnalysis.cpp \
	lib/CodeGen/LiveVariables.cpp \
	lib/CodeGen/LLVMTargetMachine.cpp \
	lib/CodeGen/LocalStackSlotAllocation.cpp \
	lib/CodeGen/MachineBasicBlock.cpp \
	lib/CodeGen/MachineBlockFrequencyInfo.cpp \
	lib/CodeGen/MachineBranchProbabilityInfo.cpp \
	lib/CodeGen/MachineCSE.cpp \
	lib/CodeGen/MachineDominators.cpp \
	lib/CodeGen/MachineFunctionAnalysis.cpp \
	lib/CodeGen/MachineFunction.cpp \
	lib/CodeGen/MachineFunctionPass.cpp \
	lib/CodeGen/MachineFunctionPrinterPass.cpp \
	lib/CodeGen/MachineInstr.cpp \
	lib/CodeGen/MachineLICM.cpp \
	lib/CodeGen/MachineLoopInfo.cpp \
	lib/CodeGen/MachineLoopRanges.cpp \
	lib/CodeGen/MachineModuleInfo.cpp \
	lib/CodeGen/MachineModuleInfoImpls.cpp \
	lib/CodeGen/MachinePassRegistry.cpp \
	lib/CodeGen/MachineRegisterInfo.cpp \
	lib/CodeGen/MachineSink.cpp \
	lib/CodeGen/MachineSSAUpdater.cpp \
	lib/CodeGen/MachineVerifier.cpp \
	lib/CodeGen/ObjectCodeEmitter.cpp \
	lib/CodeGen/OcamlGC.cpp \
	lib/CodeGen/OptimizePHIs.cpp \
	lib/CodeGen/Passes.cpp \
	lib/CodeGen/PeepholeOptimizer.cpp \
	lib/CodeGen/PHIElimination.cpp \
	lib/CodeGen/PHIEliminationUtils.cpp \
	lib/CodeGen/PostRASchedulerList.cpp \
	lib/CodeGen/ProcessImplicitDefs.cpp \
	lib/CodeGen/PrologEpilogInserter.cpp \
	lib/CodeGen/PseudoSourceValue.cpp \
	lib/CodeGen/RegAllocBasic.cpp \
	lib/CodeGen/RegAllocFast.cpp \
	lib/CodeGen/RegAllocGreedy.cpp \
	lib/CodeGen/RegAllocLinearScan.cpp \
	lib/CodeGen/RegAllocPBQP.cpp \
	lib/CodeGen/RegisterClassInfo.cpp \
	lib/CodeGen/RegisterCoalescer.cpp \
	lib/CodeGen/RegisterScavenging.cpp \
	lib/CodeGen/RenderMachineFunction.cpp \
	lib/CodeGen/ScheduleDAG.cpp \
	lib/CodeGen/ScheduleDAGEmit.cpp \
	lib/CodeGen/ScheduleDAGInstrs.cpp \
	lib/CodeGen/ScheduleDAGPrinter.cpp \
	lib/CodeGen/ScoreboardHazardRecognizer.cpp \
	lib/CodeGen/ShadowStackGC.cpp \
	lib/CodeGen/ShrinkWrapping.cpp \
	lib/CodeGen/SjLjEHPrepare.cpp \
	lib/CodeGen/SlotIndexes.cpp \
	lib/CodeGen/Spiller.cpp \
	lib/CodeGen/SpillPlacement.cpp \
	lib/CodeGen/SplitKit.cpp \
	lib/CodeGen/Splitter.cpp \
	lib/CodeGen/StackProtector.cpp \
	lib/CodeGen/StackSlotColoring.cpp \
	lib/CodeGen/StrongPHIElimination.cpp \
	lib/CodeGen/TailDuplication.cpp \
	lib/CodeGen/TargetInstrInfoImpl.cpp \
	lib/CodeGen/TargetLoweringObjectFileImpl.cpp \
	lib/CodeGen/TwoAddressInstructionPass.cpp \
	lib/CodeGen/UnreachableBlockElim.cpp \
	lib/CodeGen/VirtRegMap.cpp \
	lib/CodeGen/VirtRegRewriter.cpp \

LOCAL_SRC_FILES += \
	lib/ExecutionEngine/JIT/Intercept.cpp \
	lib/ExecutionEngine/JIT/JIT.cpp \
	lib/ExecutionEngine/JIT/JITDebugRegisterer.cpp \
	lib/ExecutionEngine/JIT/JITDwarfEmitter.cpp \
	lib/ExecutionEngine/JIT/JITEmitter.cpp \
	lib/ExecutionEngine/JIT/JITMemoryManager.cpp \
	lib/ExecutionEngine/JIT/OProfileJITEventListener.cpp \
	lib/ExecutionEngine/ExecutionEngine.cpp \
	lib/ExecutionEngine/TargetSelect.cpp \

LOCAL_SRC_FILES += \
	lib/MC/ELFObjectWriter.cpp \
	lib/MC/MachObjectWriter.cpp \
	lib/MC/MCAsmBackend.cpp \
	lib/MC/MCAsmInfoCOFF.cpp \
	lib/MC/MCAsmInfo.cpp \
	lib/MC/MCAsmInfoDarwin.cpp \
	lib/MC/MCAsmStreamer.cpp \
	lib/MC/MCAssembler.cpp \
	lib/MC/MCAtom.cpp \
	lib/MC/MCCodeEmitter.cpp \
	lib/MC/MCCodeGenInfo.cpp \
	lib/MC/MCContext.cpp \
	lib/MC/MCDisassembler.cpp \
	lib/MC/MCDwarf.cpp \
	lib/MC/MCELF.cpp \
	lib/MC/MCELFObjectTargetWriter.cpp \
	lib/MC/MCELFStreamer.cpp \
	lib/MC/MCExpr.cpp \
	lib/MC/MCInst.cpp \
	lib/MC/MCInstPrinter.cpp \
	lib/MC/MCInstrAnalysis.cpp \
	lib/MC/MCLabel.cpp \
	lib/MC/MCLoggingStreamer.cpp \
	lib/MC/MCMachObjectTargetWriter.cpp \
	lib/MC/MCMachOStreamer.cpp \
	lib/MC/MCModule.cpp \
	lib/MC/MCNullStreamer.cpp \
	lib/MC/MCObjectFileInfo.cpp \
	lib/MC/MCObjectStreamer.cpp \
	lib/MC/MCObjectWriter.cpp \
	lib/MC/MCPureStreamer.cpp \
	lib/MC/MCSectionCOFF.cpp \
	lib/MC/MCSection.cpp \
	lib/MC/MCSectionELF.cpp \
	lib/MC/MCSectionMachO.cpp \
	lib/MC/MCStreamer.cpp \
	lib/MC/MCSubtargetInfo.cpp \
	lib/MC/MCSymbol.cpp \
	lib/MC/MCTargetAsmLexer.cpp \
	lib/MC/MCValue.cpp \
	lib/MC/MCWin64EH.cpp \
	lib/MC/SubtargetFeature.cpp \
	lib/MC/WinCOFFObjectWriter.cpp \
	lib/MC/WinCOFFStreamer.cpp \

LOCAL_SRC_FILES += \
	lib/Support/Allocator.cpp \
	lib/Support/APFloat.cpp \
	lib/Support/APInt.cpp \
	lib/Support/APSInt.cpp \
	lib/Support/Atomic.cpp \
	lib/Support/BlockFrequency.cpp \
	lib/Support/BranchProbability.cpp \
	lib/Support/circular_raw_ostream.cpp \
	lib/Support/CommandLine.cpp \
	lib/Support/ConstantRange.cpp \
	lib/Support/CrashRecoveryContext.cpp \
	lib/Support/DAGDeltaAlgorithm.cpp \
	lib/Support/DataExtractor.cpp \
	lib/Support/Debug.cpp \
	lib/Support/DeltaAlgorithm.cpp \
	lib/Support/Disassembler.cpp \
	lib/Support/Dwarf.cpp \
	lib/Support/DynamicLibrary.cpp \
	lib/Support/Errno.cpp \
	lib/Support/ErrorHandling.cpp \
	lib/Support/FileUtilities.cpp \
	lib/Support/FoldingSet.cpp \
	lib/Support/FormattedStream.cpp \
	lib/Support/GraphWriter.cpp \
	lib/Support/Host.cpp \
	lib/Support/IncludeFile.cpp \
	lib/Support/IntEqClasses.cpp \
	lib/Support/IntervalMap.cpp \
	lib/Support/IsInf.cpp \
	lib/Support/IsNAN.cpp \
	lib/Support/ManagedStatic.cpp \
	lib/Support/MemoryBuffer.cpp \
	lib/Support/Memory.cpp \
	lib/Support/MemoryObject.cpp \
	lib/Support/Mutex.cpp \
	lib/Support/Path.cpp \
	lib/Support/PathV2.cpp \
	lib/Support/PluginLoader.cpp \
	lib/Support/PrettyStackTrace.cpp \
	lib/Support/Process.cpp \
	lib/Support/Program.cpp \
	lib/Support/raw_os_ostream.cpp \
	lib/Support/raw_ostream.cpp \
	lib/Support/Regex.cpp \
	lib/Support/RWMutex.cpp \
	lib/Support/SearchForAddressOfSpecialSymbol.cpp \
	lib/Support/Signals.cpp \
	lib/Support/SmallPtrSet.cpp \
	lib/Support/SmallVector.cpp \
	lib/Support/SourceMgr.cpp \
	lib/Support/Statistic.cpp \
	lib/Support/StringExtras.cpp \
	lib/Support/StringMap.cpp \
	lib/Support/StringPool.cpp \
	lib/Support/StringRef.cpp \
	lib/Support/system_error.cpp \
	lib/Support/SystemUtils.cpp \
	lib/Support/TargetRegistry.cpp \
	lib/Support/Threading.cpp \
	lib/Support/ThreadLocal.cpp \
	lib/Support/Timer.cpp \
	lib/Support/TimeValue.cpp \
	lib/Support/ToolOutputFile.cpp \
	lib/Support/Triple.cpp \
	lib/Support/Twine.cpp \
	lib/Support/Valgrind.cpp \

LOCAL_SRC_FILES += \
	lib/Target/X86/InstPrinter/X86ATTInstPrinter.cpp \
	lib/Target/X86/InstPrinter/X86InstComments.cpp \
	lib/Target/X86/InstPrinter/X86IntelInstPrinter.cpp \
	lib/Target/X86/MCTargetDesc/X86AsmBackend.cpp \
	lib/Target/X86/MCTargetDesc/X86MachObjectWriter.cpp \
	lib/Target/X86/MCTargetDesc/X86MCAsmInfo.cpp \
	lib/Target/X86/MCTargetDesc/X86MCCodeEmitter.cpp \
	lib/Target/X86/MCTargetDesc/X86MCTargetDesc.cpp \
	lib/Target/X86/TargetInfo/X86TargetInfo.cpp \
	lib/Target/X86/Utils/X86ShuffleDecode.cpp \
	lib/Target/X86/X86CodeEmitter.cpp \
	lib/Target/X86/X86ELFWriterInfo.cpp \
	lib/Target/X86/X86FastISel.cpp \
	lib/Target/X86/X86FloatingPoint.cpp \
	lib/Target/X86/X86FrameLowering.cpp \
	lib/Target/X86/X86InstrInfo.cpp \
	lib/Target/X86/X86ISelDAGToDAG.cpp \
	lib/Target/X86/X86ISelLowering.cpp \
	lib/Target/X86/X86JITInfo.cpp \
	lib/Target/X86/X86RegisterInfo.cpp \
	lib/Target/X86/X86SelectionDAGInfo.cpp \
	lib/Target/X86/X86Subtarget.cpp \
	lib/Target/X86/X86TargetMachine.cpp \
	lib/Target/X86/X86TargetObjectFile.cpp \
	lib/Target/X86/X86VZeroUpper.cpp \
	lib/Target/Mangler.cpp \
	lib/Target/Target.cpp \
	lib/Target/TargetData.cpp \
	lib/Target/TargetELFWriterInfo.cpp \
	lib/Target/TargetFrameLowering.cpp \
	lib/Target/TargetInstrInfo.cpp \
	lib/Target/TargetLibraryInfo.cpp \
	lib/Target/TargetLoweringObjectFile.cpp \
	lib/Target/TargetMachine.cpp \
	lib/Target/TargetRegisterInfo.cpp \
	lib/Target/TargetSubtargetInfo.cpp \

LOCAL_SRC_FILES += \
	lib/Transforms/InstCombine/InstCombineAddSub.cpp \
	lib/Transforms/InstCombine/InstCombineAndOrXor.cpp \
	lib/Transforms/InstCombine/InstCombineCalls.cpp \
	lib/Transforms/InstCombine/InstCombineCasts.cpp \
	lib/Transforms/InstCombine/InstCombineCompares.cpp \
	lib/Transforms/InstCombine/InstCombineLoadStoreAlloca.cpp \
	lib/Transforms/InstCombine/InstCombineMulDivRem.cpp \
	lib/Transforms/InstCombine/InstCombinePHI.cpp \
	lib/Transforms/InstCombine/InstCombineSelect.cpp \
	lib/Transforms/InstCombine/InstCombineShifts.cpp \
	lib/Transforms/InstCombine/InstCombineSimplifyDemanded.cpp \
	lib/Transforms/InstCombine/InstCombineVectorOps.cpp \
	lib/Transforms/InstCombine/InstructionCombining.cpp \
	lib/Transforms/Scalar/ADCE.cpp \
	lib/Transforms/Scalar/CodeGenPrepare.cpp \
	lib/Transforms/Scalar/DeadStoreElimination.cpp \
	lib/Transforms/Scalar/GVN.cpp \
	lib/Transforms/Scalar/LICM.cpp \
	lib/Transforms/Scalar/LoopStrengthReduce.cpp \
	lib/Transforms/Scalar/Reassociate.cpp \
	lib/Transforms/Scalar/Reg2Mem.cpp \
	lib/Transforms/Scalar/ScalarReplAggregates.cpp \
	lib/Transforms/Scalar/SCCP.cpp \
	lib/Transforms/Scalar/SimplifyCFGPass.cpp \
	lib/Transforms/Utils/AddrModeMatcher.cpp \
	lib/Transforms/Utils/BasicBlockUtils.cpp \
	lib/Transforms/Utils/BreakCriticalEdges.cpp \
	lib/Transforms/Utils/BuildLibCalls.cpp \
	lib/Transforms/Utils/DemoteRegToStack.cpp \
	lib/Transforms/Utils/InstructionNamer.cpp \
	lib/Transforms/Utils/LCSSA.cpp \
	lib/Transforms/Utils/Local.cpp \
	lib/Transforms/Utils/LoopSimplify.cpp \
	lib/Transforms/Utils/LowerInvoke.cpp \
	lib/Transforms/Utils/LowerSwitch.cpp \
	lib/Transforms/Utils/Mem2Reg.cpp \
	lib/Transforms/Utils/PromoteMemoryToRegister.cpp \
	lib/Transforms/Utils/SimplifyCFG.cpp \
	lib/Transforms/Utils/SSAUpdater.cpp \
	lib/Transforms/Utils/UnifyFunctionExitNodes.cpp \

LOCAL_SRC_FILES += \
	lib/VMCore/AsmWriter.cpp \
	lib/VMCore/Attributes.cpp \
	lib/VMCore/AutoUpgrade.cpp \
	lib/VMCore/BasicBlock.cpp \
	lib/VMCore/ConstantFold.cpp \
	lib/VMCore/Constants.cpp \
	lib/VMCore/Core.cpp \
	lib/VMCore/DebugInfoProbe.cpp \
	lib/VMCore/DebugLoc.cpp \
	lib/VMCore/Dominators.cpp \
	lib/VMCore/Function.cpp \
	lib/VMCore/GCOV.cpp \
	lib/VMCore/Globals.cpp \
	lib/VMCore/GVMaterializer.cpp \
	lib/VMCore/InlineAsm.cpp \
	lib/VMCore/Instruction.cpp \
	lib/VMCore/Instructions.cpp \
	lib/VMCore/IntrinsicInst.cpp \
	lib/VMCore/IRBuilder.cpp \
	lib/VMCore/LeakDetector.cpp \
	lib/VMCore/LLVMContext.cpp \
	lib/VMCore/LLVMContextImpl.cpp \
	lib/VMCore/Metadata.cpp \
	lib/VMCore/Module.cpp \
	lib/VMCore/Pass.cpp \
	lib/VMCore/PassManager.cpp \
	lib/VMCore/PassRegistry.cpp \
	lib/VMCore/PrintModulePass.cpp \
	lib/VMCore/Type.cpp \
	lib/VMCore/Use.cpp \
	lib/VMCore/User.cpp \
	lib/VMCore/Value.cpp \
	lib/VMCore/ValueSymbolTable.cpp \
	lib/VMCore/ValueTypes.cpp \
	lib/VMCore/Verifier.cpp \

LOCAL_CFLAGS += \
	-DLOG_TAG=\"libLLVM_swiftshader\" \
	-Wall \
	-Werror \
	-Wno-implicit-exception-spec-mismatch \
	-Wno-overloaded-virtual \
	-Wno-undefined-var-template \
	-Wno-unneeded-internal-declaration \
	-Wno-unused-const-variable \
	-Wno-unused-function \
	-Wno-unused-local-typedef \
	-Wno-unused-parameter \
	-Wno-unused-private-field \
	-Wno-unused-variable \
	-Wno-unknown-warning-option

ifneq (16,${PLATFORM_SDK_VERSION})
LOCAL_CFLAGS += -Xclang -fuse-init-array
else
LOCAL_CFLAGS += -D__STDC_INT64__
endif

ifeq (19,${PLATFORM_SDK_VERSION})
# The compiler that shipped with K had false positives for missing sentinels
LOCAL_CFLAGS += -Wno-sentinel
endif

LOCAL_CFLAGS += -fomit-frame-pointer -Os -ffunction-sections -fdata-sections
LOCAL_CFLAGS += -fno-operator-names -msse2 -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS
LOCAL_CFLAGS += -std=c++11
LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

# Android's make system also uses NDEBUG, so we need to set/unset it forcefully
# Uncomment for debug ON:
# LOCAL_CFLAGS += -UNDEBUG -g -O0

LOCAL_C_INCLUDES += \
	bionic \
	$(LOCAL_PATH)/include-android \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/lib/Target/X86 \

# Marshmallow does not have stlport, but comes with libc++ by default
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -lt 23 && echo PreMarshmallow),PreMarshmallow)
LOCAL_C_INCLUDES += external/stlport/stlport
endif

include $(BUILD_STATIC_LIBRARY)
