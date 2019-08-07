//===-- MipsISelLowering.cpp - Mips DAG Lowering Implementation -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Mips uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "mips-lower"
#include "MipsISelLowering.h"
#include "MipsMachineFunction.h"
#include "MipsTargetMachine.h"
#include "MipsTargetObjectFile.h"
#include "MipsSubtarget.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Intrinsics.h"
#include "llvm/CallingConv.h"
#include "InstPrinter/MipsInstPrinter.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

// If I is a shifted mask, set the size (Size) and the first bit of the 
// mask (Pos), and return true.
// For example, if I is 0x003ff800, (Pos, Size) = (11, 11).  
static bool IsShiftedMask(uint64_t I, uint64_t &Pos, uint64_t &Size) {
  if (!isUInt<32>(I) || !isShiftedMask_32(I))
     return false;

  Size = CountPopulation_32(I);
  Pos = CountTrailingZeros_32(I);
  return true;
}

const char *MipsTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case MipsISD::JmpLink:           return "MipsISD::JmpLink";
  case MipsISD::Hi:                return "MipsISD::Hi";
  case MipsISD::Lo:                return "MipsISD::Lo";
  case MipsISD::GPRel:             return "MipsISD::GPRel";
  case MipsISD::TlsGd:             return "MipsISD::TlsGd";
  case MipsISD::TprelHi:           return "MipsISD::TprelHi";
  case MipsISD::TprelLo:           return "MipsISD::TprelLo";
  case MipsISD::ThreadPointer:     return "MipsISD::ThreadPointer";
  case MipsISD::Ret:               return "MipsISD::Ret";
  case MipsISD::FPBrcond:          return "MipsISD::FPBrcond";
  case MipsISD::FPCmp:             return "MipsISD::FPCmp";
  case MipsISD::CMovFP_T:          return "MipsISD::CMovFP_T";
  case MipsISD::CMovFP_F:          return "MipsISD::CMovFP_F";
  case MipsISD::FPRound:           return "MipsISD::FPRound";
  case MipsISD::MAdd:              return "MipsISD::MAdd";
  case MipsISD::MAddu:             return "MipsISD::MAddu";
  case MipsISD::MSub:              return "MipsISD::MSub";
  case MipsISD::MSubu:             return "MipsISD::MSubu";
  case MipsISD::DivRem:            return "MipsISD::DivRem";
  case MipsISD::DivRemU:           return "MipsISD::DivRemU";
  case MipsISD::BuildPairF64:      return "MipsISD::BuildPairF64";
  case MipsISD::ExtractElementF64: return "MipsISD::ExtractElementF64";
  case MipsISD::WrapperPIC:        return "MipsISD::WrapperPIC";
  case MipsISD::DynAlloc:          return "MipsISD::DynAlloc";
  case MipsISD::Sync:              return "MipsISD::Sync";
  case MipsISD::Ext:               return "MipsISD::Ext";
  case MipsISD::Ins:               return "MipsISD::Ins";
  default:                         return NULL;
  }
}

MipsTargetLowering::
MipsTargetLowering(MipsTargetMachine &TM)
  : TargetLowering(TM, new MipsTargetObjectFile()),
    Subtarget(&TM.getSubtarget<MipsSubtarget>()),
    HasMips64(Subtarget->hasMips64()), IsN64(Subtarget->isABI_N64()) {

  // Mips does not have i1 type, so use i32 for
  // setcc operations results (slt, sgt, ...).
  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent); // FIXME: Is this correct?

  // Set up the register classes
  addRegisterClass(MVT::i32, Mips::CPURegsRegisterClass);
  addRegisterClass(MVT::f32, Mips::FGR32RegisterClass);

  if (HasMips64)
    addRegisterClass(MVT::i64, Mips::CPU64RegsRegisterClass);

  // When dealing with single precision only, use libcalls
  if (!Subtarget->isSingleFloat()) {
    if (HasMips64)
      addRegisterClass(MVT::f64, Mips::FGR64RegisterClass);
    else
      addRegisterClass(MVT::f64, Mips::AFGR64RegisterClass);
  }

  // Load extented operations for i1 types must be promoted
  setLoadExtAction(ISD::EXTLOAD,  MVT::i1,  Promote);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i1,  Promote);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i1,  Promote);

  // MIPS doesn't have extending float->double load/store
  setLoadExtAction(ISD::EXTLOAD, MVT::f32, Expand);
  setTruncStoreAction(MVT::f64, MVT::f32, Expand);

  // Used by legalize types to correctly generate the setcc result.
  // Without this, every float setcc comes with a AND/OR with the result,
  // we don't want this, since the fpcmp result goes to a flag register,
  // which is used implicitly by brcond and select operations.
  AddPromotedToType(ISD::SETCC, MVT::i1, MVT::i32);

  // Mips Custom Operations
  setOperationAction(ISD::GlobalAddress,      MVT::i32,   Custom);
  setOperationAction(ISD::GlobalAddress,      MVT::i64,   Custom);
  setOperationAction(ISD::BlockAddress,       MVT::i32,   Custom);
  setOperationAction(ISD::GlobalTLSAddress,   MVT::i32,   Custom);
  setOperationAction(ISD::JumpTable,          MVT::i32,   Custom);
  setOperationAction(ISD::ConstantPool,       MVT::i32,   Custom);
  setOperationAction(ISD::SELECT,             MVT::f32,   Custom);
  setOperationAction(ISD::SELECT,             MVT::f64,   Custom);
  setOperationAction(ISD::SELECT,             MVT::i32,   Custom);
  setOperationAction(ISD::BRCOND,             MVT::Other, Custom);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32,   Custom);
  setOperationAction(ISD::VASTART,            MVT::Other, Custom);

  setOperationAction(ISD::SDIV, MVT::i32, Expand);
  setOperationAction(ISD::SREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIV, MVT::i32, Expand);
  setOperationAction(ISD::UREM, MVT::i32, Expand);
  setOperationAction(ISD::SDIV, MVT::i64, Expand);
  setOperationAction(ISD::SREM, MVT::i64, Expand);
  setOperationAction(ISD::UDIV, MVT::i64, Expand);
  setOperationAction(ISD::UREM, MVT::i64, Expand);

  // Operations not directly supported by Mips.
  setOperationAction(ISD::BR_JT,             MVT::Other, Expand);
  setOperationAction(ISD::BR_CC,             MVT::Other, Expand);
  setOperationAction(ISD::SELECT_CC,         MVT::Other, Expand);
  setOperationAction(ISD::UINT_TO_FP,        MVT::i32,   Expand);
  setOperationAction(ISD::FP_TO_UINT,        MVT::i32,   Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1,    Expand);
  setOperationAction(ISD::CTPOP,             MVT::i32,   Expand);
  setOperationAction(ISD::CTTZ,              MVT::i32,   Expand);
  setOperationAction(ISD::ROTL,              MVT::i32,   Expand);
  setOperationAction(ISD::ROTL,              MVT::i64,   Expand);

  if (!Subtarget->hasMips32r2())
    setOperationAction(ISD::ROTR, MVT::i32,   Expand);

  if (!Subtarget->hasMips64r2())
    setOperationAction(ISD::ROTR, MVT::i64,   Expand);

  setOperationAction(ISD::SHL_PARTS,         MVT::i32,   Expand);
  setOperationAction(ISD::SRA_PARTS,         MVT::i32,   Expand);
  setOperationAction(ISD::SRL_PARTS,         MVT::i32,   Expand);
  setOperationAction(ISD::FCOPYSIGN,         MVT::f32,   Custom);
  setOperationAction(ISD::FCOPYSIGN,         MVT::f64,   Custom);
  setOperationAction(ISD::FSIN,              MVT::f32,   Expand);
  setOperationAction(ISD::FSIN,              MVT::f64,   Expand);
  setOperationAction(ISD::FCOS,              MVT::f32,   Expand);
  setOperationAction(ISD::FCOS,              MVT::f64,   Expand);
  setOperationAction(ISD::FPOWI,             MVT::f32,   Expand);
  setOperationAction(ISD::FPOW,              MVT::f32,   Expand);
  setOperationAction(ISD::FPOW,              MVT::f64,   Expand);
  setOperationAction(ISD::FLOG,              MVT::f32,   Expand);
  setOperationAction(ISD::FLOG2,             MVT::f32,   Expand);
  setOperationAction(ISD::FLOG10,            MVT::f32,   Expand);
  setOperationAction(ISD::FEXP,              MVT::f32,   Expand);
  setOperationAction(ISD::FMA,               MVT::f32,   Expand);
  setOperationAction(ISD::FMA,               MVT::f64,   Expand);

  setOperationAction(ISD::EXCEPTIONADDR,     MVT::i32, Expand);
  setOperationAction(ISD::EHSELECTION,       MVT::i32, Expand);

  setOperationAction(ISD::VAARG,             MVT::Other, Expand);
  setOperationAction(ISD::VACOPY,            MVT::Other, Expand);
  setOperationAction(ISD::VAEND,             MVT::Other, Expand);

  // Use the default for now
  setOperationAction(ISD::STACKSAVE,         MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE,      MVT::Other, Expand);

  setOperationAction(ISD::MEMBARRIER,        MVT::Other, Custom);
  setOperationAction(ISD::ATOMIC_FENCE,      MVT::Other, Custom);  

  setOperationAction(ISD::ATOMIC_LOAD,       MVT::i32,    Expand);  
  setOperationAction(ISD::ATOMIC_STORE,      MVT::i32,    Expand);  

  setInsertFencesForAtomic(true);

  if (Subtarget->isSingleFloat())
    setOperationAction(ISD::SELECT_CC, MVT::f64, Expand);

  if (!Subtarget->hasSEInReg()) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8,  Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  }

  if (!Subtarget->hasBitCount())
    setOperationAction(ISD::CTLZ, MVT::i32, Expand);

  if (!Subtarget->hasSwap())
    setOperationAction(ISD::BSWAP, MVT::i32, Expand);

  setTargetDAGCombine(ISD::ADDE);
  setTargetDAGCombine(ISD::SUBE);
  setTargetDAGCombine(ISD::SDIVREM);
  setTargetDAGCombine(ISD::UDIVREM);
  setTargetDAGCombine(ISD::SETCC);
  setTargetDAGCombine(ISD::AND);
  setTargetDAGCombine(ISD::OR);

  setMinFunctionAlignment(2);

  setStackPointerRegisterToSaveRestore(Mips::SP);
  computeRegisterProperties();

  setExceptionPointerRegister(Mips::A0);
  setExceptionSelectorRegister(Mips::A1);
}

bool MipsTargetLowering::allowsUnalignedMemoryAccesses(EVT VT) const {
  MVT::SimpleValueType SVT = VT.getSimpleVT().SimpleTy;
  return SVT == MVT::i64 || SVT == MVT::i32 || SVT == MVT::i16; 
}

EVT MipsTargetLowering::getSetCCResultType(EVT VT) const {
  return MVT::i32;
}

// SelectMadd -
// Transforms a subgraph in CurDAG if the following pattern is found:
//  (addc multLo, Lo0), (adde multHi, Hi0),
// where,
//  multHi/Lo: product of multiplication
//  Lo0: initial value of Lo register
//  Hi0: initial value of Hi register
// Return true if pattern matching was successful.
static bool SelectMadd(SDNode* ADDENode, SelectionDAG* CurDAG) {
  // ADDENode's second operand must be a flag output of an ADDC node in order
  // for the matching to be successful.
  SDNode* ADDCNode = ADDENode->getOperand(2).getNode();

  if (ADDCNode->getOpcode() != ISD::ADDC)
    return false;

  SDValue MultHi = ADDENode->getOperand(0);
  SDValue MultLo = ADDCNode->getOperand(0);
  SDNode* MultNode = MultHi.getNode();
  unsigned MultOpc = MultHi.getOpcode();

  // MultHi and MultLo must be generated by the same node,
  if (MultLo.getNode() != MultNode)
    return false;

  // and it must be a multiplication.
  if (MultOpc != ISD::SMUL_LOHI && MultOpc != ISD::UMUL_LOHI)
    return false;

  // MultLo amd MultHi must be the first and second output of MultNode
  // respectively.
  if (MultHi.getResNo() != 1 || MultLo.getResNo() != 0)
    return false;

  // Transform this to a MADD only if ADDENode and ADDCNode are the only users
  // of the values of MultNode, in which case MultNode will be removed in later
  // phases.
  // If there exist users other than ADDENode or ADDCNode, this function returns
  // here, which will result in MultNode being mapped to a single MULT
  // instruction node rather than a pair of MULT and MADD instructions being
  // produced.
  if (!MultHi.hasOneUse() || !MultLo.hasOneUse())
    return false;

  SDValue Chain = CurDAG->getEntryNode();
  DebugLoc dl = ADDENode->getDebugLoc();

  // create MipsMAdd(u) node
  MultOpc = MultOpc == ISD::UMUL_LOHI ? MipsISD::MAddu : MipsISD::MAdd;

  SDValue MAdd = CurDAG->getNode(MultOpc, dl,
                                 MVT::Glue,
                                 MultNode->getOperand(0),// Factor 0
                                 MultNode->getOperand(1),// Factor 1
                                 ADDCNode->getOperand(1),// Lo0
                                 ADDENode->getOperand(1));// Hi0

  // create CopyFromReg nodes
  SDValue CopyFromLo = CurDAG->getCopyFromReg(Chain, dl, Mips::LO, MVT::i32,
                                              MAdd);
  SDValue CopyFromHi = CurDAG->getCopyFromReg(CopyFromLo.getValue(1), dl,
                                              Mips::HI, MVT::i32,
                                              CopyFromLo.getValue(2));

  // replace uses of adde and addc here
  if (!SDValue(ADDCNode, 0).use_empty())
    CurDAG->ReplaceAllUsesOfValueWith(SDValue(ADDCNode, 0), CopyFromLo);

  if (!SDValue(ADDENode, 0).use_empty())
    CurDAG->ReplaceAllUsesOfValueWith(SDValue(ADDENode, 0), CopyFromHi);

  return true;
}

// SelectMsub -
// Transforms a subgraph in CurDAG if the following pattern is found:
//  (addc Lo0, multLo), (sube Hi0, multHi),
// where,
//  multHi/Lo: product of multiplication
//  Lo0: initial value of Lo register
//  Hi0: initial value of Hi register
// Return true if pattern matching was successful.
static bool SelectMsub(SDNode* SUBENode, SelectionDAG* CurDAG) {
  // SUBENode's second operand must be a flag output of an SUBC node in order
  // for the matching to be successful.
  SDNode* SUBCNode = SUBENode->getOperand(2).getNode();

  if (SUBCNode->getOpcode() != ISD::SUBC)
    return false;

  SDValue MultHi = SUBENode->getOperand(1);
  SDValue MultLo = SUBCNode->getOperand(1);
  SDNode* MultNode = MultHi.getNode();
  unsigned MultOpc = MultHi.getOpcode();

  // MultHi and MultLo must be generated by the same node,
  if (MultLo.getNode() != MultNode)
    return false;

  // and it must be a multiplication.
  if (MultOpc != ISD::SMUL_LOHI && MultOpc != ISD::UMUL_LOHI)
    return false;

  // MultLo amd MultHi must be the first and second output of MultNode
  // respectively.
  if (MultHi.getResNo() != 1 || MultLo.getResNo() != 0)
    return false;

  // Transform this to a MSUB only if SUBENode and SUBCNode are the only users
  // of the values of MultNode, in which case MultNode will be removed in later
  // phases.
  // If there exist users other than SUBENode or SUBCNode, this function returns
  // here, which will result in MultNode being mapped to a single MULT
  // instruction node rather than a pair of MULT and MSUB instructions being
  // produced.
  if (!MultHi.hasOneUse() || !MultLo.hasOneUse())
    return false;

  SDValue Chain = CurDAG->getEntryNode();
  DebugLoc dl = SUBENode->getDebugLoc();

  // create MipsSub(u) node
  MultOpc = MultOpc == ISD::UMUL_LOHI ? MipsISD::MSubu : MipsISD::MSub;

  SDValue MSub = CurDAG->getNode(MultOpc, dl,
                                 MVT::Glue,
                                 MultNode->getOperand(0),// Factor 0
                                 MultNode->getOperand(1),// Factor 1
                                 SUBCNode->getOperand(0),// Lo0
                                 SUBENode->getOperand(0));// Hi0

  // create CopyFromReg nodes
  SDValue CopyFromLo = CurDAG->getCopyFromReg(Chain, dl, Mips::LO, MVT::i32,
                                              MSub);
  SDValue CopyFromHi = CurDAG->getCopyFromReg(CopyFromLo.getValue(1), dl,
                                              Mips::HI, MVT::i32,
                                              CopyFromLo.getValue(2));

  // replace uses of sube and subc here
  if (!SDValue(SUBCNode, 0).use_empty())
    CurDAG->ReplaceAllUsesOfValueWith(SDValue(SUBCNode, 0), CopyFromLo);

  if (!SDValue(SUBENode, 0).use_empty())
    CurDAG->ReplaceAllUsesOfValueWith(SDValue(SUBENode, 0), CopyFromHi);

  return true;
}

static SDValue PerformADDECombine(SDNode *N, SelectionDAG& DAG,
                                  TargetLowering::DAGCombinerInfo &DCI,
                                  const MipsSubtarget* Subtarget) {
  if (DCI.isBeforeLegalize())
    return SDValue();

  if (Subtarget->hasMips32() && SelectMadd(N, &DAG))
    return SDValue(N, 0);

  return SDValue();
}

static SDValue PerformSUBECombine(SDNode *N, SelectionDAG& DAG,
                                  TargetLowering::DAGCombinerInfo &DCI,
                                  const MipsSubtarget* Subtarget) {
  if (DCI.isBeforeLegalize())
    return SDValue();

  if (Subtarget->hasMips32() && SelectMsub(N, &DAG))
    return SDValue(N, 0);

  return SDValue();
}

static SDValue PerformDivRemCombine(SDNode *N, SelectionDAG& DAG,
                                    TargetLowering::DAGCombinerInfo &DCI,
                                    const MipsSubtarget* Subtarget) {
  if (DCI.isBeforeLegalizeOps())
    return SDValue();

  EVT Ty = N->getValueType(0);
  unsigned LO = (Ty == MVT::i32) ? Mips::LO : Mips::LO64; 
  unsigned HI = (Ty == MVT::i32) ? Mips::HI : Mips::HI64; 
  unsigned opc = N->getOpcode() == ISD::SDIVREM ? MipsISD::DivRem :
                                                  MipsISD::DivRemU;
  DebugLoc dl = N->getDebugLoc();

  SDValue DivRem = DAG.getNode(opc, dl, MVT::Glue,
                               N->getOperand(0), N->getOperand(1));
  SDValue InChain = DAG.getEntryNode();
  SDValue InGlue = DivRem;

  // insert MFLO
  if (N->hasAnyUseOfValue(0)) {
    SDValue CopyFromLo = DAG.getCopyFromReg(InChain, dl, LO, Ty,
                                            InGlue);
    DAG.ReplaceAllUsesOfValueWith(SDValue(N, 0), CopyFromLo);
    InChain = CopyFromLo.getValue(1);
    InGlue = CopyFromLo.getValue(2);
  }

  // insert MFHI
  if (N->hasAnyUseOfValue(1)) {
    SDValue CopyFromHi = DAG.getCopyFromReg(InChain, dl,
                                            HI, Ty, InGlue);
    DAG.ReplaceAllUsesOfValueWith(SDValue(N, 1), CopyFromHi);
  }

  return SDValue();
}

static Mips::CondCode FPCondCCodeToFCC(ISD::CondCode CC) {
  switch (CC) {
  default: llvm_unreachable("Unknown fp condition code!");
  case ISD::SETEQ:
  case ISD::SETOEQ: return Mips::FCOND_OEQ;
  case ISD::SETUNE: return Mips::FCOND_UNE;
  case ISD::SETLT:
  case ISD::SETOLT: return Mips::FCOND_OLT;
  case ISD::SETGT:
  case ISD::SETOGT: return Mips::FCOND_OGT;
  case ISD::SETLE:
  case ISD::SETOLE: return Mips::FCOND_OLE;
  case ISD::SETGE:
  case ISD::SETOGE: return Mips::FCOND_OGE;
  case ISD::SETULT: return Mips::FCOND_ULT;
  case ISD::SETULE: return Mips::FCOND_ULE;
  case ISD::SETUGT: return Mips::FCOND_UGT;
  case ISD::SETUGE: return Mips::FCOND_UGE;
  case ISD::SETUO:  return Mips::FCOND_UN;
  case ISD::SETO:   return Mips::FCOND_OR;
  case ISD::SETNE:
  case ISD::SETONE: return Mips::FCOND_ONE;
  case ISD::SETUEQ: return Mips::FCOND_UEQ;
  }
}


// Returns true if condition code has to be inverted.
static bool InvertFPCondCode(Mips::CondCode CC) {
  if (CC >= Mips::FCOND_F && CC <= Mips::FCOND_NGT)
    return false;

  if (CC >= Mips::FCOND_T && CC <= Mips::FCOND_GT)
    return true;

  assert(false && "Illegal Condition Code");
  return false;
}

// Creates and returns an FPCmp node from a setcc node.
// Returns Op if setcc is not a floating point comparison.
static SDValue CreateFPCmp(SelectionDAG& DAG, const SDValue& Op) {
  // must be a SETCC node
  if (Op.getOpcode() != ISD::SETCC)
    return Op;

  SDValue LHS = Op.getOperand(0);

  if (!LHS.getValueType().isFloatingPoint())
    return Op;

  SDValue RHS = Op.getOperand(1);
  DebugLoc dl = Op.getDebugLoc();

  // Assume the 3rd operand is a CondCodeSDNode. Add code to check the type of
  // node if necessary.
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  return DAG.getNode(MipsISD::FPCmp, dl, MVT::Glue, LHS, RHS,
                     DAG.getConstant(FPCondCCodeToFCC(CC), MVT::i32));
}

// Creates and returns a CMovFPT/F node.
static SDValue CreateCMovFP(SelectionDAG& DAG, SDValue Cond, SDValue True,
                            SDValue False, DebugLoc DL) {
  bool invert = InvertFPCondCode((Mips::CondCode)
                                 cast<ConstantSDNode>(Cond.getOperand(2))
                                 ->getSExtValue());

  return DAG.getNode((invert ? MipsISD::CMovFP_F : MipsISD::CMovFP_T), DL,
                     True.getValueType(), True, False, Cond);
}

static SDValue PerformSETCCCombine(SDNode *N, SelectionDAG& DAG,
                                   TargetLowering::DAGCombinerInfo &DCI,
                                   const MipsSubtarget* Subtarget) {
  if (DCI.isBeforeLegalizeOps())
    return SDValue();

  SDValue Cond = CreateFPCmp(DAG, SDValue(N, 0));

  if (Cond.getOpcode() != MipsISD::FPCmp)
    return SDValue();

  SDValue True  = DAG.getConstant(1, MVT::i32);
  SDValue False = DAG.getConstant(0, MVT::i32);

  return CreateCMovFP(DAG, Cond, True, False, N->getDebugLoc());
}

static SDValue PerformANDCombine(SDNode *N, SelectionDAG& DAG,
                                 TargetLowering::DAGCombinerInfo &DCI,
                                 const MipsSubtarget* Subtarget) {
  // Pattern match EXT.
  //  $dst = and ((sra or srl) $src , pos), (2**size - 1)
  //  => ext $dst, $src, size, pos
  if (DCI.isBeforeLegalizeOps() || !Subtarget->hasMips32r2())
    return SDValue();

  SDValue ShiftRight = N->getOperand(0), Mask = N->getOperand(1);
  
  // Op's first operand must be a shift right.
  if (ShiftRight.getOpcode() != ISD::SRA && ShiftRight.getOpcode() != ISD::SRL)
    return SDValue();

  // The second operand of the shift must be an immediate.
  uint64_t Pos;
  ConstantSDNode *CN;
  if (!(CN = dyn_cast<ConstantSDNode>(ShiftRight.getOperand(1))))
    return SDValue();
  
  Pos = CN->getZExtValue();

  uint64_t SMPos, SMSize;
  // Op's second operand must be a shifted mask.
  if (!(CN = dyn_cast<ConstantSDNode>(Mask)) ||
      !IsShiftedMask(CN->getZExtValue(), SMPos, SMSize))
    return SDValue();

  // Return if the shifted mask does not start at bit 0 or the sum of its size
  // and Pos exceeds the word's size.
  if (SMPos != 0 || Pos + SMSize > 32)
    return SDValue();

  return DAG.getNode(MipsISD::Ext, N->getDebugLoc(), MVT::i32,
                     ShiftRight.getOperand(0),
                     DAG.getConstant(Pos, MVT::i32),
                     DAG.getConstant(SMSize, MVT::i32));
}
  
static SDValue PerformORCombine(SDNode *N, SelectionDAG& DAG,
                                TargetLowering::DAGCombinerInfo &DCI,
                                const MipsSubtarget* Subtarget) {
  // Pattern match INS.
  //  $dst = or (and $src1 , mask0), (and (shl $src, pos), mask1),
  //  where mask1 = (2**size - 1) << pos, mask0 = ~mask1 
  //  => ins $dst, $src, size, pos, $src1
  if (DCI.isBeforeLegalizeOps() || !Subtarget->hasMips32r2())
    return SDValue();

  SDValue And0 = N->getOperand(0), And1 = N->getOperand(1);
  uint64_t SMPos0, SMSize0, SMPos1, SMSize1;
  ConstantSDNode *CN;

  // See if Op's first operand matches (and $src1 , mask0).
  if (And0.getOpcode() != ISD::AND)
    return SDValue();

  if (!(CN = dyn_cast<ConstantSDNode>(And0.getOperand(1))) ||
      !IsShiftedMask(~CN->getSExtValue(), SMPos0, SMSize0))
    return SDValue();

  // See if Op's second operand matches (and (shl $src, pos), mask1).
  if (And1.getOpcode() != ISD::AND)
    return SDValue();
  
  if (!(CN = dyn_cast<ConstantSDNode>(And1.getOperand(1))) ||
      !IsShiftedMask(CN->getZExtValue(), SMPos1, SMSize1))
    return SDValue();

  // The shift masks must have the same position and size.
  if (SMPos0 != SMPos1 || SMSize0 != SMSize1)
    return SDValue();

  SDValue Shl = And1.getOperand(0);
  if (Shl.getOpcode() != ISD::SHL)
    return SDValue();

  if (!(CN = dyn_cast<ConstantSDNode>(Shl.getOperand(1))))
    return SDValue();

  unsigned Shamt = CN->getZExtValue();

  // Return if the shift amount and the first bit position of mask are not the
  // same.  
  if (Shamt != SMPos0)
    return SDValue();
  
  return DAG.getNode(MipsISD::Ins, N->getDebugLoc(), MVT::i32,
                     Shl.getOperand(0),
                     DAG.getConstant(SMPos0, MVT::i32),
                     DAG.getConstant(SMSize0, MVT::i32),
                     And0.getOperand(0));  
}
  
SDValue  MipsTargetLowering::PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI)
  const {
  SelectionDAG &DAG = DCI.DAG;
  unsigned opc = N->getOpcode();

  switch (opc) {
  default: break;
  case ISD::ADDE:
    return PerformADDECombine(N, DAG, DCI, Subtarget);
  case ISD::SUBE:
    return PerformSUBECombine(N, DAG, DCI, Subtarget);
  case ISD::SDIVREM:
  case ISD::UDIVREM:
    return PerformDivRemCombine(N, DAG, DCI, Subtarget);
  case ISD::SETCC:
    return PerformSETCCCombine(N, DAG, DCI, Subtarget);
  case ISD::AND:
    return PerformANDCombine(N, DAG, DCI, Subtarget);
  case ISD::OR:
    return PerformORCombine(N, DAG, DCI, Subtarget);
  }

  return SDValue();
}

SDValue MipsTargetLowering::
LowerOperation(SDValue Op, SelectionDAG &DAG) const
{
  switch (Op.getOpcode())
  {
    case ISD::BRCOND:             return LowerBRCOND(Op, DAG);
    case ISD::ConstantPool:       return LowerConstantPool(Op, DAG);
    case ISD::DYNAMIC_STACKALLOC: return LowerDYNAMIC_STACKALLOC(Op, DAG);
    case ISD::GlobalAddress:      return LowerGlobalAddress(Op, DAG);
    case ISD::BlockAddress:       return LowerBlockAddress(Op, DAG);
    case ISD::GlobalTLSAddress:   return LowerGlobalTLSAddress(Op, DAG);
    case ISD::JumpTable:          return LowerJumpTable(Op, DAG);
    case ISD::SELECT:             return LowerSELECT(Op, DAG);
    case ISD::VASTART:            return LowerVASTART(Op, DAG);
    case ISD::FCOPYSIGN:          return LowerFCOPYSIGN(Op, DAG);
    case ISD::FRAMEADDR:          return LowerFRAMEADDR(Op, DAG);
    case ISD::MEMBARRIER:         return LowerMEMBARRIER(Op, DAG);
    case ISD::ATOMIC_FENCE:       return LowerATOMIC_FENCE(Op, DAG);
  }
  return SDValue();
}

//===----------------------------------------------------------------------===//
//  Lower helper functions
//===----------------------------------------------------------------------===//

// AddLiveIn - This helper function adds the specified physical register to the
// MachineFunction as a live in value.  It also creates a corresponding
// virtual register for it.
static unsigned
AddLiveIn(MachineFunction &MF, unsigned PReg, TargetRegisterClass *RC)
{
  assert(RC->contains(PReg) && "Not the correct regclass!");
  unsigned VReg = MF.getRegInfo().createVirtualRegister(RC);
  MF.getRegInfo().addLiveIn(PReg, VReg);
  return VReg;
}

// Get fp branch code (not opcode) from condition code.
static Mips::FPBranchCode GetFPBranchCodeFromCond(Mips::CondCode CC) {
  if (CC >= Mips::FCOND_F && CC <= Mips::FCOND_NGT)
    return Mips::BRANCH_T;

  if (CC >= Mips::FCOND_T && CC <= Mips::FCOND_GT)
    return Mips::BRANCH_F;

  return Mips::BRANCH_INVALID;
}

static MachineBasicBlock* ExpandCondMov(MachineInstr *MI, MachineBasicBlock *BB,
                                        DebugLoc dl,
                                        const MipsSubtarget* Subtarget,
                                        const TargetInstrInfo *TII,
                                        bool isFPCmp, unsigned Opc) {
  // There is no need to expand CMov instructions if target has
  // conditional moves.
  if (Subtarget->hasCondMov())
    return BB;

  // To "insert" a SELECT_CC instruction, we actually have to insert the
  // diamond control-flow pattern.  The incoming instruction knows the
  // destination vreg to set, the condition code register to branch on, the
  // true/false values to select between, and a branch opcode to use.
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = BB;
  ++It;

  //  thisMBB:
  //  ...
  //   TrueVal = ...
  //   setcc r1, r2, r3
  //   bNE   r1, r0, copy1MBB
  //   fallthrough --> copy0MBB
  MachineBasicBlock *thisMBB  = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *copy0MBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *sinkMBB  = F->CreateMachineBasicBlock(LLVM_BB);
  F->insert(It, copy0MBB);
  F->insert(It, sinkMBB);

  // Transfer the remainder of BB and its successor edges to sinkMBB.
  sinkMBB->splice(sinkMBB->begin(), BB,
                  llvm::next(MachineBasicBlock::iterator(MI)),
                  BB->end());
  sinkMBB->transferSuccessorsAndUpdatePHIs(BB);

  // Next, add the true and fallthrough blocks as its successors.
  BB->addSuccessor(copy0MBB);
  BB->addSuccessor(sinkMBB);

  // Emit the right instruction according to the type of the operands compared
  if (isFPCmp)
    BuildMI(BB, dl, TII->get(Opc)).addMBB(sinkMBB);
  else
    BuildMI(BB, dl, TII->get(Opc)).addReg(MI->getOperand(2).getReg())
      .addReg(Mips::ZERO).addMBB(sinkMBB);

  //  copy0MBB:
  //   %FalseValue = ...
  //   # fallthrough to sinkMBB
  BB = copy0MBB;

  // Update machine-CFG edges
  BB->addSuccessor(sinkMBB);

  //  sinkMBB:
  //   %Result = phi [ %TrueValue, thisMBB ], [ %FalseValue, copy0MBB ]
  //  ...
  BB = sinkMBB;

  if (isFPCmp)
    BuildMI(*BB, BB->begin(), dl,
            TII->get(Mips::PHI), MI->getOperand(0).getReg())
      .addReg(MI->getOperand(2).getReg()).addMBB(thisMBB)
      .addReg(MI->getOperand(1).getReg()).addMBB(copy0MBB);
  else
    BuildMI(*BB, BB->begin(), dl,
            TII->get(Mips::PHI), MI->getOperand(0).getReg())
      .addReg(MI->getOperand(3).getReg()).addMBB(thisMBB)
      .addReg(MI->getOperand(1).getReg()).addMBB(copy0MBB);

  MI->eraseFromParent();   // The pseudo instruction is gone now.
  return BB;
}

MachineBasicBlock *
MipsTargetLowering::EmitInstrWithCustomInserter(MachineInstr *MI,
                                                MachineBasicBlock *BB) const {
  const TargetInstrInfo *TII = getTargetMachine().getInstrInfo();
  DebugLoc dl = MI->getDebugLoc();

  switch (MI->getOpcode()) {
  default:
    assert(false && "Unexpected instr type to insert");
    return NULL;
  case Mips::MOVT:
  case Mips::MOVT_S:
  case Mips::MOVT_D:
    return ExpandCondMov(MI, BB, dl, Subtarget, TII, true, Mips::BC1F);
  case Mips::MOVF:
  case Mips::MOVF_S:
  case Mips::MOVF_D:
    return ExpandCondMov(MI, BB, dl, Subtarget, TII, true, Mips::BC1T);
  case Mips::MOVZ_I:
  case Mips::MOVZ_S:
  case Mips::MOVZ_D:
    return ExpandCondMov(MI, BB, dl, Subtarget, TII, false, Mips::BNE);
  case Mips::MOVN_I:
  case Mips::MOVN_S:
  case Mips::MOVN_D:
    return ExpandCondMov(MI, BB, dl, Subtarget, TII, false, Mips::BEQ);

  case Mips::ATOMIC_LOAD_ADD_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, Mips::ADDu);
  case Mips::ATOMIC_LOAD_ADD_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, Mips::ADDu);
  case Mips::ATOMIC_LOAD_ADD_I32:
    return EmitAtomicBinary(MI, BB, 4, Mips::ADDu);

  case Mips::ATOMIC_LOAD_AND_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, Mips::AND);
  case Mips::ATOMIC_LOAD_AND_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, Mips::AND);
  case Mips::ATOMIC_LOAD_AND_I32:
    return EmitAtomicBinary(MI, BB, 4, Mips::AND);

  case Mips::ATOMIC_LOAD_OR_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, Mips::OR);
  case Mips::ATOMIC_LOAD_OR_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, Mips::OR);
  case Mips::ATOMIC_LOAD_OR_I32:
    return EmitAtomicBinary(MI, BB, 4, Mips::OR);

  case Mips::ATOMIC_LOAD_XOR_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, Mips::XOR);
  case Mips::ATOMIC_LOAD_XOR_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, Mips::XOR);
  case Mips::ATOMIC_LOAD_XOR_I32:
    return EmitAtomicBinary(MI, BB, 4, Mips::XOR);

  case Mips::ATOMIC_LOAD_NAND_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, 0, true);
  case Mips::ATOMIC_LOAD_NAND_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, 0, true);
  case Mips::ATOMIC_LOAD_NAND_I32:
    return EmitAtomicBinary(MI, BB, 4, 0, true);

  case Mips::ATOMIC_LOAD_SUB_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, Mips::SUBu);
  case Mips::ATOMIC_LOAD_SUB_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, Mips::SUBu);
  case Mips::ATOMIC_LOAD_SUB_I32:
    return EmitAtomicBinary(MI, BB, 4, Mips::SUBu);

  case Mips::ATOMIC_SWAP_I8:
    return EmitAtomicBinaryPartword(MI, BB, 1, 0);
  case Mips::ATOMIC_SWAP_I16:
    return EmitAtomicBinaryPartword(MI, BB, 2, 0);
  case Mips::ATOMIC_SWAP_I32:
    return EmitAtomicBinary(MI, BB, 4, 0);

  case Mips::ATOMIC_CMP_SWAP_I8:
    return EmitAtomicCmpSwapPartword(MI, BB, 1);
  case Mips::ATOMIC_CMP_SWAP_I16:
    return EmitAtomicCmpSwapPartword(MI, BB, 2);
  case Mips::ATOMIC_CMP_SWAP_I32:
    return EmitAtomicCmpSwap(MI, BB, 4);
  }
}

// This function also handles Mips::ATOMIC_SWAP_I32 (when BinOpcode == 0), and
// Mips::ATOMIC_LOAD_NAND_I32 (when Nand == true)
MachineBasicBlock *
MipsTargetLowering::EmitAtomicBinary(MachineInstr *MI, MachineBasicBlock *BB,
                                     unsigned Size, unsigned BinOpcode,
                                     bool Nand) const {
  assert(Size == 4 && "Unsupported size for EmitAtomicBinary.");

  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &RegInfo = MF->getRegInfo();
  const TargetRegisterClass *RC = getRegClassFor(MVT::i32);
  const TargetInstrInfo *TII = getTargetMachine().getInstrInfo();
  DebugLoc dl = MI->getDebugLoc();

  unsigned OldVal = MI->getOperand(0).getReg();
  unsigned Ptr = MI->getOperand(1).getReg();
  unsigned Incr = MI->getOperand(2).getReg();

  unsigned StoreVal = RegInfo.createVirtualRegister(RC);
  unsigned AndRes = RegInfo.createVirtualRegister(RC);
  unsigned Success = RegInfo.createVirtualRegister(RC);

  // insert new blocks after the current block
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineBasicBlock *loopMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *exitMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineFunction::iterator It = BB;
  ++It;
  MF->insert(It, loopMBB);
  MF->insert(It, exitMBB);

  // Transfer the remainder of BB and its successor edges to exitMBB.
  exitMBB->splice(exitMBB->begin(), BB,
                  llvm::next(MachineBasicBlock::iterator(MI)),
                  BB->end());
  exitMBB->transferSuccessorsAndUpdatePHIs(BB);

  //  thisMBB:
  //    ...
  //    fallthrough --> loopMBB
  BB->addSuccessor(loopMBB);
  loopMBB->addSuccessor(loopMBB);
  loopMBB->addSuccessor(exitMBB);

  //  loopMBB:
  //    ll oldval, 0(ptr)
  //    <binop> storeval, oldval, incr
  //    sc success, storeval, 0(ptr)
  //    beq success, $0, loopMBB
  BB = loopMBB;
  BuildMI(BB, dl, TII->get(Mips::LL), OldVal).addReg(Ptr).addImm(0);
  if (Nand) {
    //  and andres, oldval, incr
    //  nor storeval, $0, andres
    BuildMI(BB, dl, TII->get(Mips::AND), AndRes).addReg(OldVal).addReg(Incr);
    BuildMI(BB, dl, TII->get(Mips::NOR), StoreVal)
      .addReg(Mips::ZERO).addReg(AndRes);
  } else if (BinOpcode) {
    //  <binop> storeval, oldval, incr
    BuildMI(BB, dl, TII->get(BinOpcode), StoreVal).addReg(OldVal).addReg(Incr);
  } else {
    StoreVal = Incr;
  }
  BuildMI(BB, dl, TII->get(Mips::SC), Success)
    .addReg(StoreVal).addReg(Ptr).addImm(0);
  BuildMI(BB, dl, TII->get(Mips::BEQ))
    .addReg(Success).addReg(Mips::ZERO).addMBB(loopMBB);

  MI->eraseFromParent();   // The instruction is gone now.

  return exitMBB;
}

MachineBasicBlock *
MipsTargetLowering::EmitAtomicBinaryPartword(MachineInstr *MI,
                                             MachineBasicBlock *BB,
                                             unsigned Size, unsigned BinOpcode,
                                             bool Nand) const {
  assert((Size == 1 || Size == 2) &&
      "Unsupported size for EmitAtomicBinaryPartial.");

  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &RegInfo = MF->getRegInfo();
  const TargetRegisterClass *RC = getRegClassFor(MVT::i32);
  const TargetInstrInfo *TII = getTargetMachine().getInstrInfo();
  DebugLoc dl = MI->getDebugLoc();

  unsigned Dest = MI->getOperand(0).getReg();
  unsigned Ptr = MI->getOperand(1).getReg();
  unsigned Incr = MI->getOperand(2).getReg();

  unsigned AlignedAddr = RegInfo.createVirtualRegister(RC);
  unsigned ShiftAmt = RegInfo.createVirtualRegister(RC);
  unsigned Mask = RegInfo.createVirtualRegister(RC);
  unsigned Mask2 = RegInfo.createVirtualRegister(RC);
  unsigned NewVal = RegInfo.createVirtualRegister(RC);
  unsigned OldVal = RegInfo.createVirtualRegister(RC);
  unsigned Incr2 = RegInfo.createVirtualRegister(RC);
  unsigned MaskLSB2 = RegInfo.createVirtualRegister(RC);
  unsigned PtrLSB2 = RegInfo.createVirtualRegister(RC);
  unsigned MaskUpper = RegInfo.createVirtualRegister(RC);
  unsigned AndRes = RegInfo.createVirtualRegister(RC);
  unsigned BinOpRes = RegInfo.createVirtualRegister(RC);
  unsigned MaskedOldVal0 = RegInfo.createVirtualRegister(RC);
  unsigned StoreVal = RegInfo.createVirtualRegister(RC);
  unsigned MaskedOldVal1 = RegInfo.createVirtualRegister(RC);
  unsigned SrlRes = RegInfo.createVirtualRegister(RC);
  unsigned SllRes = RegInfo.createVirtualRegister(RC);
  unsigned Success = RegInfo.createVirtualRegister(RC);

  // insert new blocks after the current block
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineBasicBlock *loopMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *sinkMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *exitMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineFunction::iterator It = BB;
  ++It;
  MF->insert(It, loopMBB);
  MF->insert(It, sinkMBB);
  MF->insert(It, exitMBB);

  // Transfer the remainder of BB and its successor edges to exitMBB.
  exitMBB->splice(exitMBB->begin(), BB,
                  llvm::next(MachineBasicBlock::iterator(MI)),
                  BB->end());
  exitMBB->transferSuccessorsAndUpdatePHIs(BB);

  BB->addSuccessor(loopMBB);
  loopMBB->addSuccessor(loopMBB);
  loopMBB->addSuccessor(sinkMBB);
  sinkMBB->addSuccessor(exitMBB);

  //  thisMBB:
  //    addiu   masklsb2,$0,-4                # 0xfffffffc
  //    and     alignedaddr,ptr,masklsb2
  //    andi    ptrlsb2,ptr,3
  //    sll     shiftamt,ptrlsb2,3
  //    ori     maskupper,$0,255               # 0xff
  //    sll     mask,maskupper,shiftamt
  //    nor     mask2,$0,mask
  //    sll     incr2,incr,shiftamt

  int64_t MaskImm = (Size == 1) ? 255 : 65535;
  BuildMI(BB, dl, TII->get(Mips::ADDiu), MaskLSB2)
    .addReg(Mips::ZERO).addImm(-4);
  BuildMI(BB, dl, TII->get(Mips::AND), AlignedAddr)
    .addReg(Ptr).addReg(MaskLSB2);
  BuildMI(BB, dl, TII->get(Mips::ANDi), PtrLSB2).addReg(Ptr).addImm(3);
  BuildMI(BB, dl, TII->get(Mips::SLL), ShiftAmt).addReg(PtrLSB2).addImm(3);
  BuildMI(BB, dl, TII->get(Mips::ORi), MaskUpper)
    .addReg(Mips::ZERO).addImm(MaskImm);
  BuildMI(BB, dl, TII->get(Mips::SLLV), Mask)
    .addReg(ShiftAmt).addReg(MaskUpper);
  BuildMI(BB, dl, TII->get(Mips::NOR), Mask2).addReg(Mips::ZERO).addReg(Mask);
  BuildMI(BB, dl, TII->get(Mips::SLLV), Incr2).addReg(ShiftAmt).addReg(Incr);


  // atomic.load.binop
  // loopMBB:
  //   ll      oldval,0(alignedaddr)
  //   binop   binopres,oldval,incr2
  //   and     newval,binopres,mask
  //   and     maskedoldval0,oldval,mask2
  //   or      storeval,maskedoldval0,newval
  //   sc      success,storeval,0(alignedaddr)
  //   beq     success,$0,loopMBB

  // atomic.swap
  // loopMBB:
  //   ll      oldval,0(alignedaddr)
  //   and     newval,incr2,mask
  //   and     maskedoldval0,oldval,mask2
  //   or      storeval,maskedoldval0,newval
  //   sc      success,storeval,0(alignedaddr)
  //   beq     success,$0,loopMBB

  BB = loopMBB;
  BuildMI(BB, dl, TII->get(Mips::LL), OldVal).addReg(AlignedAddr).addImm(0);
  if (Nand) {
    //  and andres, oldval, incr2
    //  nor binopres, $0, andres
    //  and newval, binopres, mask
    BuildMI(BB, dl, TII->get(Mips::AND), AndRes).addReg(OldVal).addReg(Incr2);
    BuildMI(BB, dl, TII->get(Mips::NOR), BinOpRes)
      .addReg(Mips::ZERO).addReg(AndRes);
    BuildMI(BB, dl, TII->get(Mips::AND), NewVal).addReg(BinOpRes).addReg(Mask);
  } else if (BinOpcode) {
    //  <binop> binopres, oldval, incr2
    //  and newval, binopres, mask
    BuildMI(BB, dl, TII->get(BinOpcode), BinOpRes).addReg(OldVal).addReg(Incr2);
    BuildMI(BB, dl, TII->get(Mips::AND), NewVal).addReg(BinOpRes).addReg(Mask);
  } else {// atomic.swap
    //  and newval, incr2, mask
    BuildMI(BB, dl, TII->get(Mips::AND), NewVal).addReg(Incr2).addReg(Mask);
  }
    
  BuildMI(BB, dl, TII->get(Mips::AND), MaskedOldVal0)
    .addReg(OldVal).addReg(Mask2);
  BuildMI(BB, dl, TII->get(Mips::OR), StoreVal)
    .addReg(MaskedOldVal0).addReg(NewVal);
  BuildMI(BB, dl, TII->get(Mips::SC), Success)
    .addReg(StoreVal).addReg(AlignedAddr).addImm(0);
  BuildMI(BB, dl, TII->get(Mips::BEQ))
    .addReg(Success).addReg(Mips::ZERO).addMBB(loopMBB);

  //  sinkMBB:
  //    and     maskedoldval1,oldval,mask
  //    srl     srlres,maskedoldval1,shiftamt
  //    sll     sllres,srlres,24
  //    sra     dest,sllres,24
  BB = sinkMBB;
  int64_t ShiftImm = (Size == 1) ? 24 : 16;

  BuildMI(BB, dl, TII->get(Mips::AND), MaskedOldVal1)
    .addReg(OldVal).addReg(Mask);
  BuildMI(BB, dl, TII->get(Mips::SRLV), SrlRes)
      .addReg(ShiftAmt).addReg(MaskedOldVal1);
  BuildMI(BB, dl, TII->get(Mips::SLL), SllRes)
      .addReg(SrlRes).addImm(ShiftImm);
  BuildMI(BB, dl, TII->get(Mips::SRA), Dest)
      .addReg(SllRes).addImm(ShiftImm);

  MI->eraseFromParent();   // The instruction is gone now.

  return exitMBB;
}

MachineBasicBlock *
MipsTargetLowering::EmitAtomicCmpSwap(MachineInstr *MI,
                                      MachineBasicBlock *BB,
                                      unsigned Size) const {
  assert(Size == 4 && "Unsupported size for EmitAtomicCmpSwap.");

  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &RegInfo = MF->getRegInfo();
  const TargetRegisterClass *RC = getRegClassFor(MVT::i32);
  const TargetInstrInfo *TII = getTargetMachine().getInstrInfo();
  DebugLoc dl = MI->getDebugLoc();

  unsigned Dest    = MI->getOperand(0).getReg();
  unsigned Ptr     = MI->getOperand(1).getReg();
  unsigned OldVal  = MI->getOperand(2).getReg();
  unsigned NewVal  = MI->getOperand(3).getReg();

  unsigned Success = RegInfo.createVirtualRegister(RC);

  // insert new blocks after the current block
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineBasicBlock *loop1MBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *loop2MBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *exitMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineFunction::iterator It = BB;
  ++It;
  MF->insert(It, loop1MBB);
  MF->insert(It, loop2MBB);
  MF->insert(It, exitMBB);

  // Transfer the remainder of BB and its successor edges to exitMBB.
  exitMBB->splice(exitMBB->begin(), BB,
                  llvm::next(MachineBasicBlock::iterator(MI)),
                  BB->end());
  exitMBB->transferSuccessorsAndUpdatePHIs(BB);

  //  thisMBB:
  //    ...
  //    fallthrough --> loop1MBB
  BB->addSuccessor(loop1MBB);
  loop1MBB->addSuccessor(exitMBB);
  loop1MBB->addSuccessor(loop2MBB);
  loop2MBB->addSuccessor(loop1MBB);
  loop2MBB->addSuccessor(exitMBB);

  // loop1MBB:
  //   ll dest, 0(ptr)
  //   bne dest, oldval, exitMBB
  BB = loop1MBB;
  BuildMI(BB, dl, TII->get(Mips::LL), Dest).addReg(Ptr).addImm(0);
  BuildMI(BB, dl, TII->get(Mips::BNE))
    .addReg(Dest).addReg(OldVal).addMBB(exitMBB);

  // loop2MBB:
  //   sc success, newval, 0(ptr)
  //   beq success, $0, loop1MBB
  BB = loop2MBB;
  BuildMI(BB, dl, TII->get(Mips::SC), Success)
    .addReg(NewVal).addReg(Ptr).addImm(0);
  BuildMI(BB, dl, TII->get(Mips::BEQ))
    .addReg(Success).addReg(Mips::ZERO).addMBB(loop1MBB);

  MI->eraseFromParent();   // The instruction is gone now.

  return exitMBB;
}

MachineBasicBlock *
MipsTargetLowering::EmitAtomicCmpSwapPartword(MachineInstr *MI,
                                              MachineBasicBlock *BB,
                                              unsigned Size) const {
  assert((Size == 1 || Size == 2) &&
      "Unsupported size for EmitAtomicCmpSwapPartial.");

  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &RegInfo = MF->getRegInfo();
  const TargetRegisterClass *RC = getRegClassFor(MVT::i32);
  const TargetInstrInfo *TII = getTargetMachine().getInstrInfo();
  DebugLoc dl = MI->getDebugLoc();

  unsigned Dest    = MI->getOperand(0).getReg();
  unsigned Ptr     = MI->getOperand(1).getReg();
  unsigned CmpVal  = MI->getOperand(2).getReg();
  unsigned NewVal  = MI->getOperand(3).getReg();

  unsigned AlignedAddr = RegInfo.createVirtualRegister(RC);
  unsigned ShiftAmt = RegInfo.createVirtualRegister(RC);
  unsigned Mask = RegInfo.createVirtualRegister(RC);
  unsigned Mask2 = RegInfo.createVirtualRegister(RC);
  unsigned ShiftedCmpVal = RegInfo.createVirtualRegister(RC);
  unsigned OldVal = RegInfo.createVirtualRegister(RC);
  unsigned MaskedOldVal0 = RegInfo.createVirtualRegister(RC);
  unsigned ShiftedNewVal = RegInfo.createVirtualRegister(RC);
  unsigned MaskLSB2 = RegInfo.createVirtualRegister(RC);
  unsigned PtrLSB2 = RegInfo.createVirtualRegister(RC);
  unsigned MaskUpper = RegInfo.createVirtualRegister(RC);
  unsigned MaskedCmpVal = RegInfo.createVirtualRegister(RC);
  unsigned MaskedNewVal = RegInfo.createVirtualRegister(RC);
  unsigned MaskedOldVal1 = RegInfo.createVirtualRegister(RC);
  unsigned StoreVal = RegInfo.createVirtualRegister(RC);
  unsigned SrlRes = RegInfo.createVirtualRegister(RC);
  unsigned SllRes = RegInfo.createVirtualRegister(RC);
  unsigned Success = RegInfo.createVirtualRegister(RC);

  // insert new blocks after the current block
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineBasicBlock *loop1MBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *loop2MBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *sinkMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *exitMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineFunction::iterator It = BB;
  ++It;
  MF->insert(It, loop1MBB);
  MF->insert(It, loop2MBB);
  MF->insert(It, sinkMBB);
  MF->insert(It, exitMBB);

  // Transfer the remainder of BB and its successor edges to exitMBB.
  exitMBB->splice(exitMBB->begin(), BB,
                  llvm::next(MachineBasicBlock::iterator(MI)),
                  BB->end());
  exitMBB->transferSuccessorsAndUpdatePHIs(BB);

  BB->addSuccessor(loop1MBB);
  loop1MBB->addSuccessor(sinkMBB);
  loop1MBB->addSuccessor(loop2MBB);
  loop2MBB->addSuccessor(loop1MBB);
  loop2MBB->addSuccessor(sinkMBB);
  sinkMBB->addSuccessor(exitMBB);

  // FIXME: computation of newval2 can be moved to loop2MBB.
  //  thisMBB:
  //    addiu   masklsb2,$0,-4                # 0xfffffffc
  //    and     alignedaddr,ptr,masklsb2
  //    andi    ptrlsb2,ptr,3
  //    sll     shiftamt,ptrlsb2,3
  //    ori     maskupper,$0,255               # 0xff
  //    sll     mask,maskupper,shiftamt
  //    nor     mask2,$0,mask
  //    andi    maskedcmpval,cmpval,255
  //    sll     shiftedcmpval,maskedcmpval,shiftamt
  //    andi    maskednewval,newval,255
  //    sll     shiftednewval,maskednewval,shiftamt
  int64_t MaskImm = (Size == 1) ? 255 : 65535;
  BuildMI(BB, dl, TII->get(Mips::ADDiu), MaskLSB2)
    .addReg(Mips::ZERO).addImm(-4);
  BuildMI(BB, dl, TII->get(Mips::AND), AlignedAddr)
    .addReg(Ptr).addReg(MaskLSB2);
  BuildMI(BB, dl, TII->get(Mips::ANDi), PtrLSB2).addReg(Ptr).addImm(3);
  BuildMI(BB, dl, TII->get(Mips::SLL), ShiftAmt).addReg(PtrLSB2).addImm(3);
  BuildMI(BB, dl, TII->get(Mips::ORi), MaskUpper)
    .addReg(Mips::ZERO).addImm(MaskImm);
  BuildMI(BB, dl, TII->get(Mips::SLLV), Mask)
    .addReg(ShiftAmt).addReg(MaskUpper);
  BuildMI(BB, dl, TII->get(Mips::NOR), Mask2).addReg(Mips::ZERO).addReg(Mask);
  BuildMI(BB, dl, TII->get(Mips::ANDi), MaskedCmpVal)
    .addReg(CmpVal).addImm(MaskImm);
  BuildMI(BB, dl, TII->get(Mips::SLLV), ShiftedCmpVal)
    .addReg(ShiftAmt).addReg(MaskedCmpVal);
  BuildMI(BB, dl, TII->get(Mips::ANDi), MaskedNewVal)
    .addReg(NewVal).addImm(MaskImm);
  BuildMI(BB, dl, TII->get(Mips::SLLV), ShiftedNewVal)
    .addReg(ShiftAmt).addReg(MaskedNewVal);

  //  loop1MBB:
  //    ll      oldval,0(alginedaddr)
  //    and     maskedoldval0,oldval,mask
  //    bne     maskedoldval0,shiftedcmpval,sinkMBB
  BB = loop1MBB;
  BuildMI(BB, dl, TII->get(Mips::LL), OldVal).addReg(AlignedAddr).addImm(0);
  BuildMI(BB, dl, TII->get(Mips::AND), MaskedOldVal0)
    .addReg(OldVal).addReg(Mask);
  BuildMI(BB, dl, TII->get(Mips::BNE))
    .addReg(MaskedOldVal0).addReg(ShiftedCmpVal).addMBB(sinkMBB);

  //  loop2MBB:
  //    and     maskedoldval1,oldval,mask2
  //    or      storeval,maskedoldval1,shiftednewval
  //    sc      success,storeval,0(alignedaddr)
  //    beq     success,$0,loop1MBB
  BB = loop2MBB;
  BuildMI(BB, dl, TII->get(Mips::AND), MaskedOldVal1)
    .addReg(OldVal).addReg(Mask2);
  BuildMI(BB, dl, TII->get(Mips::OR), StoreVal)
    .addReg(MaskedOldVal1).addReg(ShiftedNewVal);
  BuildMI(BB, dl, TII->get(Mips::SC), Success)
      .addReg(StoreVal).addReg(AlignedAddr).addImm(0);
  BuildMI(BB, dl, TII->get(Mips::BEQ))
      .addReg(Success).addReg(Mips::ZERO).addMBB(loop1MBB);

  //  sinkMBB:
  //    srl     srlres,maskedoldval0,shiftamt
  //    sll     sllres,srlres,24
  //    sra     dest,sllres,24
  BB = sinkMBB;
  int64_t ShiftImm = (Size == 1) ? 24 : 16;

  BuildMI(BB, dl, TII->get(Mips::SRLV), SrlRes)
      .addReg(ShiftAmt).addReg(MaskedOldVal0);
  BuildMI(BB, dl, TII->get(Mips::SLL), SllRes)
      .addReg(SrlRes).addImm(ShiftImm);
  BuildMI(BB, dl, TII->get(Mips::SRA), Dest)
      .addReg(SllRes).addImm(ShiftImm);

  MI->eraseFromParent();   // The instruction is gone now.

  return exitMBB;
}

//===----------------------------------------------------------------------===//
//  Misc Lower Operation implementation
//===----------------------------------------------------------------------===//
SDValue MipsTargetLowering::
LowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const
{
  MachineFunction &MF = DAG.getMachineFunction();
  MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();

  assert(getTargetMachine().getFrameLowering()->getStackAlignment() >=
         cast<ConstantSDNode>(Op.getOperand(2).getNode())->getZExtValue() &&
         "Cannot lower if the alignment of the allocated space is larger than \
          that of the stack.");

  SDValue Chain = Op.getOperand(0);
  SDValue Size = Op.getOperand(1);
  DebugLoc dl = Op.getDebugLoc();

  // Get a reference from Mips stack pointer
  SDValue StackPointer = DAG.getCopyFromReg(Chain, dl, Mips::SP, MVT::i32);

  // Subtract the dynamic size from the actual stack size to
  // obtain the new stack size.
  SDValue Sub = DAG.getNode(ISD::SUB, dl, MVT::i32, StackPointer, Size);

  // The Sub result contains the new stack start address, so it
  // must be placed in the stack pointer register.
  Chain = DAG.getCopyToReg(StackPointer.getValue(1), dl, Mips::SP, Sub,
                           SDValue());

  // This node always has two return values: a new stack pointer
  // value and a chain
  SDVTList VTLs = DAG.getVTList(MVT::i32, MVT::Other);
  SDValue Ptr = DAG.getFrameIndex(MipsFI->getDynAllocFI(), getPointerTy());
  SDValue Ops[] = { Chain, Ptr, Chain.getValue(1) };

  return DAG.getNode(MipsISD::DynAlloc, dl, VTLs, Ops, 3);
}

SDValue MipsTargetLowering::
LowerBRCOND(SDValue Op, SelectionDAG &DAG) const
{
  // The first operand is the chain, the second is the condition, the third is
  // the block to branch to if the condition is true.
  SDValue Chain = Op.getOperand(0);
  SDValue Dest = Op.getOperand(2);
  DebugLoc dl = Op.getDebugLoc();

  SDValue CondRes = CreateFPCmp(DAG, Op.getOperand(1));

  // Return if flag is not set by a floating point comparison.
  if (CondRes.getOpcode() != MipsISD::FPCmp)
    return Op;

  SDValue CCNode  = CondRes.getOperand(2);
  Mips::CondCode CC =
    (Mips::CondCode)cast<ConstantSDNode>(CCNode)->getZExtValue();
  SDValue BrCode = DAG.getConstant(GetFPBranchCodeFromCond(CC), MVT::i32);

  return DAG.getNode(MipsISD::FPBrcond, dl, Op.getValueType(), Chain, BrCode,
                     Dest, CondRes);
}

SDValue MipsTargetLowering::
LowerSELECT(SDValue Op, SelectionDAG &DAG) const
{
  SDValue Cond = CreateFPCmp(DAG, Op.getOperand(0));

  // Return if flag is not set by a floating point comparison.
  if (Cond.getOpcode() != MipsISD::FPCmp)
    return Op;

  return CreateCMovFP(DAG, Cond, Op.getOperand(1), Op.getOperand(2),
                      Op.getDebugLoc());
}

SDValue MipsTargetLowering::LowerGlobalAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  // FIXME there isn't actually debug info here
  DebugLoc dl = Op.getDebugLoc();
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal(); 	

  if (getTargetMachine().getRelocationModel() != Reloc::PIC_ && !IsN64) {
    SDVTList VTs = DAG.getVTList(MVT::i32);

    MipsTargetObjectFile &TLOF = (MipsTargetObjectFile&)getObjFileLowering();

    // %gp_rel relocation
    if (TLOF.IsGlobalInSmallSection(GV, getTargetMachine())) {
      SDValue GA = DAG.getTargetGlobalAddress(GV, dl, MVT::i32, 0,
                                              MipsII::MO_GPREL);
      SDValue GPRelNode = DAG.getNode(MipsISD::GPRel, dl, VTs, &GA, 1);
      SDValue GOT = DAG.getGLOBAL_OFFSET_TABLE(MVT::i32);
      return DAG.getNode(ISD::ADD, dl, MVT::i32, GOT, GPRelNode);
    }
    // %hi/%lo relocation
    SDValue GAHi = DAG.getTargetGlobalAddress(GV, dl, MVT::i32, 0,
                                              MipsII::MO_ABS_HI);
    SDValue GALo = DAG.getTargetGlobalAddress(GV, dl, MVT::i32, 0,
                                              MipsII::MO_ABS_LO);
    SDValue HiPart = DAG.getNode(MipsISD::Hi, dl, VTs, &GAHi, 1);
    SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, GALo);
    return DAG.getNode(ISD::ADD, dl, MVT::i32, HiPart, Lo);
  }

  EVT ValTy = Op.getValueType();
  bool HasGotOfst = (GV->hasInternalLinkage() ||
                     (GV->hasLocalLinkage() && !isa<Function>(GV)));
  unsigned GotFlag = IsN64 ?
                     (HasGotOfst ? MipsII::MO_GOT_PAGE : MipsII::MO_GOT_DISP) :
                     MipsII::MO_GOT;
  SDValue GA = DAG.getTargetGlobalAddress(GV, dl, ValTy, 0, GotFlag);
  GA = DAG.getNode(MipsISD::WrapperPIC, dl, ValTy, GA);
  SDValue ResNode = DAG.getLoad(ValTy, dl,
                                DAG.getEntryNode(), GA, MachinePointerInfo(),
                                false, false, 0);
  // On functions and global targets not internal linked only
  // a load from got/GP is necessary for PIC to work.
  if (!HasGotOfst)
    return ResNode;
  SDValue GALo = DAG.getTargetGlobalAddress(GV, dl, ValTy, 0,
                                            IsN64 ? MipsII::MO_GOT_OFST :
                                                    MipsII::MO_ABS_LO);
  SDValue Lo = DAG.getNode(MipsISD::Lo, dl, ValTy, GALo);
  return DAG.getNode(ISD::ADD, dl, ValTy, ResNode, Lo);
}

SDValue MipsTargetLowering::LowerBlockAddress(SDValue Op,
                                              SelectionDAG &DAG) const {
  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  // FIXME there isn't actually debug info here
  DebugLoc dl = Op.getDebugLoc();

  if (getTargetMachine().getRelocationModel() != Reloc::PIC_) {
    // %hi/%lo relocation
    SDValue BAHi = DAG.getBlockAddress(BA, MVT::i32, true,
                                       MipsII::MO_ABS_HI);
    SDValue BALo = DAG.getBlockAddress(BA, MVT::i32, true,
                                       MipsII::MO_ABS_LO);
    SDValue Hi = DAG.getNode(MipsISD::Hi, dl, MVT::i32, BAHi);
    SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, BALo);
    return DAG.getNode(ISD::ADD, dl, MVT::i32, Hi, Lo);
  }

  SDValue BAGOTOffset = DAG.getBlockAddress(BA, MVT::i32, true,
                                            MipsII::MO_GOT);
  BAGOTOffset = DAG.getNode(MipsISD::WrapperPIC, dl, MVT::i32, BAGOTOffset);
  SDValue BALOOffset = DAG.getBlockAddress(BA, MVT::i32, true,
                                           MipsII::MO_ABS_LO);
  SDValue Load = DAG.getLoad(MVT::i32, dl,
                             DAG.getEntryNode(), BAGOTOffset,
                             MachinePointerInfo(), false, false, 0);
  SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, BALOOffset);
  return DAG.getNode(ISD::ADD, dl, MVT::i32, Load, Lo);
}

SDValue MipsTargetLowering::
LowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const
{
  // If the relocation model is PIC, use the General Dynamic TLS Model,
  // otherwise use the Initial Exec or Local Exec TLS Model.
  // TODO: implement Local Dynamic TLS model

  GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);
  DebugLoc dl = GA->getDebugLoc();
  const GlobalValue *GV = GA->getGlobal();
  EVT PtrVT = getPointerTy();

  if (getTargetMachine().getRelocationModel() == Reloc::PIC_) {
    // General Dynamic TLS Model
    SDValue TGA = DAG.getTargetGlobalAddress(GV, dl, MVT::i32,
                                             0, MipsII::MO_TLSGD);
    SDValue Tlsgd = DAG.getNode(MipsISD::TlsGd, dl, MVT::i32, TGA);
    SDValue GP = DAG.getRegister(Mips::GP, MVT::i32);
    SDValue Argument = DAG.getNode(ISD::ADD, dl, MVT::i32, GP, Tlsgd);

    ArgListTy Args;
    ArgListEntry Entry;
    Entry.Node = Argument;
    Entry.Ty = (Type *) Type::getInt32Ty(*DAG.getContext());
    Args.push_back(Entry);
    std::pair<SDValue, SDValue> CallResult =
        LowerCallTo(DAG.getEntryNode(),
                    (Type *) Type::getInt32Ty(*DAG.getContext()),
                    false, false, false, false, 0, CallingConv::C, false, true,
                    DAG.getExternalSymbol("__tls_get_addr", PtrVT), Args, DAG,
                    dl);

    return CallResult.first;
  }

  SDValue Offset;
  if (GV->isDeclaration()) {
    // Initial Exec TLS Model
    SDValue TGA = DAG.getTargetGlobalAddress(GV, dl, MVT::i32, 0,
                                             MipsII::MO_GOTTPREL);
    Offset = DAG.getLoad(MVT::i32, dl,
                         DAG.getEntryNode(), TGA, MachinePointerInfo(),
                         false, false, 0);
  } else {
    // Local Exec TLS Model
    SDVTList VTs = DAG.getVTList(MVT::i32);
    SDValue TGAHi = DAG.getTargetGlobalAddress(GV, dl, MVT::i32, 0,
                                               MipsII::MO_TPREL_HI);
    SDValue TGALo = DAG.getTargetGlobalAddress(GV, dl, MVT::i32, 0,
                                               MipsII::MO_TPREL_LO);
    SDValue Hi = DAG.getNode(MipsISD::TprelHi, dl, VTs, &TGAHi, 1);
    SDValue Lo = DAG.getNode(MipsISD::TprelLo, dl, MVT::i32, TGALo);
    Offset = DAG.getNode(ISD::ADD, dl, MVT::i32, Hi, Lo);
  }

  SDValue ThreadPointer = DAG.getNode(MipsISD::ThreadPointer, dl, PtrVT);
  return DAG.getNode(ISD::ADD, dl, PtrVT, ThreadPointer, Offset);
}

SDValue MipsTargetLowering::
LowerJumpTable(SDValue Op, SelectionDAG &DAG) const
{
  SDValue ResNode;
  SDValue HiPart;
  // FIXME there isn't actually debug info here
  DebugLoc dl = Op.getDebugLoc();
  bool IsPIC = getTargetMachine().getRelocationModel() == Reloc::PIC_;
  unsigned char OpFlag = IsPIC ? MipsII::MO_GOT : MipsII::MO_ABS_HI;

  EVT PtrVT = Op.getValueType();
  JumpTableSDNode *JT  = cast<JumpTableSDNode>(Op);

  SDValue JTI = DAG.getTargetJumpTable(JT->getIndex(), PtrVT, OpFlag);

  if (!IsPIC) {
    SDValue Ops[] = { JTI };
    HiPart = DAG.getNode(MipsISD::Hi, dl, DAG.getVTList(MVT::i32), Ops, 1);
  } else {// Emit Load from Global Pointer
    JTI = DAG.getNode(MipsISD::WrapperPIC, dl, MVT::i32, JTI);
    HiPart = DAG.getLoad(MVT::i32, dl, DAG.getEntryNode(), JTI,
                         MachinePointerInfo(),
                         false, false, 0);
  }

  SDValue JTILo = DAG.getTargetJumpTable(JT->getIndex(), PtrVT,
                                         MipsII::MO_ABS_LO);
  SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, JTILo);
  ResNode = DAG.getNode(ISD::ADD, dl, MVT::i32, HiPart, Lo);

  return ResNode;
}

SDValue MipsTargetLowering::
LowerConstantPool(SDValue Op, SelectionDAG &DAG) const
{
  SDValue ResNode;
  ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);
  const Constant *C = N->getConstVal();
  // FIXME there isn't actually debug info here
  DebugLoc dl = Op.getDebugLoc();

  // gp_rel relocation
  // FIXME: we should reference the constant pool using small data sections,
  // but the asm printer currently doesn't support this feature without
  // hacking it. This feature should come soon so we can uncomment the
  // stuff below.
  //if (IsInSmallSection(C->getType())) {
  //  SDValue GPRelNode = DAG.getNode(MipsISD::GPRel, MVT::i32, CP);
  //  SDValue GOT = DAG.getGLOBAL_OFFSET_TABLE(MVT::i32);
  //  ResNode = DAG.getNode(ISD::ADD, MVT::i32, GOT, GPRelNode);

  if (getTargetMachine().getRelocationModel() != Reloc::PIC_) {
    SDValue CPHi = DAG.getTargetConstantPool(C, MVT::i32, N->getAlignment(),
                                             N->getOffset(), MipsII::MO_ABS_HI);
    SDValue CPLo = DAG.getTargetConstantPool(C, MVT::i32, N->getAlignment(),
                                             N->getOffset(), MipsII::MO_ABS_LO);
    SDValue HiPart = DAG.getNode(MipsISD::Hi, dl, MVT::i32, CPHi);
    SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, CPLo);
    ResNode = DAG.getNode(ISD::ADD, dl, MVT::i32, HiPart, Lo);
  } else {
    SDValue CP = DAG.getTargetConstantPool(C, MVT::i32, N->getAlignment(),
                                           N->getOffset(), MipsII::MO_GOT);
    CP = DAG.getNode(MipsISD::WrapperPIC, dl, MVT::i32, CP);
    SDValue Load = DAG.getLoad(MVT::i32, dl, DAG.getEntryNode(),
                               CP, MachinePointerInfo::getConstantPool(),
                               false, false, 0);
    SDValue CPLo = DAG.getTargetConstantPool(C, MVT::i32, N->getAlignment(),
                                             N->getOffset(), MipsII::MO_ABS_LO);
    SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, CPLo);
    ResNode = DAG.getNode(ISD::ADD, dl, MVT::i32, Load, Lo);
  }

  return ResNode;
}

SDValue MipsTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MipsFunctionInfo *FuncInfo = MF.getInfo<MipsFunctionInfo>();

  DebugLoc dl = Op.getDebugLoc();
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy());

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), dl, FI, Op.getOperand(1),
                      MachinePointerInfo(SV),
                      false, false, 0);
}

static SDValue LowerFCOPYSIGN32(SDValue Op, SelectionDAG &DAG) {
  // FIXME: Use ext/ins instructions if target architecture is Mips32r2.
  DebugLoc dl = Op.getDebugLoc();
  SDValue Op0 = DAG.getNode(ISD::BITCAST, dl, MVT::i32, Op.getOperand(0));
  SDValue Op1 = DAG.getNode(ISD::BITCAST, dl, MVT::i32, Op.getOperand(1));
  SDValue And0 = DAG.getNode(ISD::AND, dl, MVT::i32, Op0,
                             DAG.getConstant(0x7fffffff, MVT::i32));
  SDValue And1 = DAG.getNode(ISD::AND, dl, MVT::i32, Op1,
                             DAG.getConstant(0x80000000, MVT::i32));
  SDValue Result = DAG.getNode(ISD::OR, dl, MVT::i32, And0, And1);
  return DAG.getNode(ISD::BITCAST, dl, MVT::f32, Result);
}

static SDValue LowerFCOPYSIGN64(SDValue Op, SelectionDAG &DAG, bool isLittle) {
  // FIXME:
  //  Use ext/ins instructions if target architecture is Mips32r2.
  //  Eliminate redundant mfc1 and mtc1 instructions.
  unsigned LoIdx = 0, HiIdx = 1;

  if (!isLittle)
    std::swap(LoIdx, HiIdx);

  DebugLoc dl = Op.getDebugLoc();
  SDValue Word0 = DAG.getNode(MipsISD::ExtractElementF64, dl, MVT::i32,
                              Op.getOperand(0),
                              DAG.getConstant(LoIdx, MVT::i32));
  SDValue Hi0 = DAG.getNode(MipsISD::ExtractElementF64, dl, MVT::i32,
                            Op.getOperand(0), DAG.getConstant(HiIdx, MVT::i32));
  SDValue Hi1 = DAG.getNode(MipsISD::ExtractElementF64, dl, MVT::i32,
                            Op.getOperand(1), DAG.getConstant(HiIdx, MVT::i32));
  SDValue And0 = DAG.getNode(ISD::AND, dl, MVT::i32, Hi0,
                             DAG.getConstant(0x7fffffff, MVT::i32));
  SDValue And1 = DAG.getNode(ISD::AND, dl, MVT::i32, Hi1,
                             DAG.getConstant(0x80000000, MVT::i32));
  SDValue Word1 = DAG.getNode(ISD::OR, dl, MVT::i32, And0, And1);

  if (!isLittle)
    std::swap(Word0, Word1);

  return DAG.getNode(MipsISD::BuildPairF64, dl, MVT::f64, Word0, Word1);
}

SDValue MipsTargetLowering::LowerFCOPYSIGN(SDValue Op, SelectionDAG &DAG)
  const {
  EVT Ty = Op.getValueType();

  assert(Ty == MVT::f32 || Ty == MVT::f64);

  if (Ty == MVT::f32)
    return LowerFCOPYSIGN32(Op, DAG);
  else
    return LowerFCOPYSIGN64(Op, DAG, Subtarget->isLittle());
}

SDValue MipsTargetLowering::
LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const {
  // check the depth
  assert((cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue() == 0) &&
         "Frame address can only be determined for current frame.");

  MachineFrameInfo *MFI = DAG.getMachineFunction().getFrameInfo();
  MFI->setFrameAddressIsTaken(true);
  EVT VT = Op.getValueType();
  DebugLoc dl = Op.getDebugLoc();
  SDValue FrameAddr = DAG.getCopyFromReg(DAG.getEntryNode(), dl, Mips::FP, VT);
  return FrameAddr;
}

// TODO: set SType according to the desired memory barrier behavior.
SDValue MipsTargetLowering::LowerMEMBARRIER(SDValue Op,
                                            SelectionDAG& DAG) const {
  unsigned SType = 0;
  DebugLoc dl = Op.getDebugLoc();
  return DAG.getNode(MipsISD::Sync, dl, MVT::Other, Op.getOperand(0),
                     DAG.getConstant(SType, MVT::i32));
}

SDValue MipsTargetLowering::LowerATOMIC_FENCE(SDValue Op,
                                              SelectionDAG& DAG) const {
  // FIXME: Need pseudo-fence for 'singlethread' fences
  // FIXME: Set SType for weaker fences where supported/appropriate.
  unsigned SType = 0;
  DebugLoc dl = Op.getDebugLoc();
  return DAG.getNode(MipsISD::Sync, dl, MVT::Other, Op.getOperand(0),
                     DAG.getConstant(SType, MVT::i32));
}

//===----------------------------------------------------------------------===//
//                      Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "MipsGenCallingConv.inc"

//===----------------------------------------------------------------------===//
// TODO: Implement a generic logic using tblgen that can support this.
// Mips O32 ABI rules:
// ---
// i32 - Passed in A0, A1, A2, A3 and stack
// f32 - Only passed in f32 registers if no int reg has been used yet to hold
//       an argument. Otherwise, passed in A1, A2, A3 and stack.
// f64 - Only passed in two aliased f32 registers if no int reg has been used
//       yet to hold an argument. Otherwise, use A2, A3 and stack. If A1 is
//       not used, it must be shadowed. If only A3 is avaiable, shadow it and
//       go to stack.
//
//  For vararg functions, all arguments are passed in A0, A1, A2, A3 and stack.
//===----------------------------------------------------------------------===//

static bool CC_MipsO32(unsigned ValNo, MVT ValVT,
                       MVT LocVT, CCValAssign::LocInfo LocInfo,
                       ISD::ArgFlagsTy ArgFlags, CCState &State) {

  static const unsigned IntRegsSize=4, FloatRegsSize=2;

  static const unsigned IntRegs[] = {
      Mips::A0, Mips::A1, Mips::A2, Mips::A3
  };
  static const unsigned F32Regs[] = {
      Mips::F12, Mips::F14
  };
  static const unsigned F64Regs[] = {
      Mips::D6, Mips::D7
  };

  // ByVal Args
  if (ArgFlags.isByVal()) {
    State.HandleByVal(ValNo, ValVT, LocVT, LocInfo,
                      1 /*MinSize*/, 4 /*MinAlign*/, ArgFlags);
    unsigned NextReg = (State.getNextStackOffset() + 3) / 4;
    for (unsigned r = State.getFirstUnallocated(IntRegs, IntRegsSize);
         r < std::min(IntRegsSize, NextReg); ++r)
      State.AllocateReg(IntRegs[r]);
    return false;
  }

  // Promote i8 and i16
  if (LocVT == MVT::i8 || LocVT == MVT::i16) {
    LocVT = MVT::i32;
    if (ArgFlags.isSExt())
      LocInfo = CCValAssign::SExt;
    else if (ArgFlags.isZExt())
      LocInfo = CCValAssign::ZExt;
    else
      LocInfo = CCValAssign::AExt;
  }

  unsigned Reg;

  // f32 and f64 are allocated in A0, A1, A2, A3 when either of the following
  // is true: function is vararg, argument is 3rd or higher, there is previous
  // argument which is not f32 or f64.
  bool AllocateFloatsInIntReg = State.isVarArg() || ValNo > 1
      || State.getFirstUnallocated(F32Regs, FloatRegsSize) != ValNo;
  unsigned OrigAlign = ArgFlags.getOrigAlign();
  bool isI64 = (ValVT == MVT::i32 && OrigAlign == 8);

  if (ValVT == MVT::i32 || (ValVT == MVT::f32 && AllocateFloatsInIntReg)) {
    Reg = State.AllocateReg(IntRegs, IntRegsSize);
    // If this is the first part of an i64 arg,
    // the allocated register must be either A0 or A2.
    if (isI64 && (Reg == Mips::A1 || Reg == Mips::A3))
      Reg = State.AllocateReg(IntRegs, IntRegsSize);
    LocVT = MVT::i32;
  } else if (ValVT == MVT::f64 && AllocateFloatsInIntReg) {
    // Allocate int register and shadow next int register. If first
    // available register is Mips::A1 or Mips::A3, shadow it too.
    Reg = State.AllocateReg(IntRegs, IntRegsSize);
    if (Reg == Mips::A1 || Reg == Mips::A3)
      Reg = State.AllocateReg(IntRegs, IntRegsSize);
    State.AllocateReg(IntRegs, IntRegsSize);
    LocVT = MVT::i32;
  } else if (ValVT.isFloatingPoint() && !AllocateFloatsInIntReg) {
    // we are guaranteed to find an available float register
    if (ValVT == MVT::f32) {
      Reg = State.AllocateReg(F32Regs, FloatRegsSize);
      // Shadow int register
      State.AllocateReg(IntRegs, IntRegsSize);
    } else {
      Reg = State.AllocateReg(F64Regs, FloatRegsSize);
      // Shadow int registers
      unsigned Reg2 = State.AllocateReg(IntRegs, IntRegsSize);
      if (Reg2 == Mips::A1 || Reg2 == Mips::A3)
        State.AllocateReg(IntRegs, IntRegsSize);
      State.AllocateReg(IntRegs, IntRegsSize);
    }
  } else
    llvm_unreachable("Cannot handle this ValVT.");

  unsigned SizeInBytes = ValVT.getSizeInBits() >> 3;
  unsigned Offset = State.AllocateStack(SizeInBytes, OrigAlign);

  if (!Reg)
    State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
  else
    State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));

  return false; // CC must always match
}

//===----------------------------------------------------------------------===//
//                  Call Calling Convention Implementation
//===----------------------------------------------------------------------===//

static const unsigned O32IntRegsSize = 4;

static const unsigned O32IntRegs[] = {
  Mips::A0, Mips::A1, Mips::A2, Mips::A3
};

// Return next O32 integer argument register.
static unsigned getNextIntArgReg(unsigned Reg) {
  assert((Reg == Mips::A0) || (Reg == Mips::A2));
  return (Reg == Mips::A0) ? Mips::A1 : Mips::A3;
}

// Write ByVal Arg to arg registers and stack.
static void
WriteByValArg(SDValue& ByValChain, SDValue Chain, DebugLoc dl,
              SmallVector<std::pair<unsigned, SDValue>, 16>& RegsToPass,
              SmallVector<SDValue, 8>& MemOpChains, int& LastFI,
              MachineFrameInfo *MFI, SelectionDAG &DAG, SDValue Arg,
              const CCValAssign &VA, const ISD::ArgFlagsTy& Flags,
              MVT PtrType, bool isLittle) {
  unsigned LocMemOffset = VA.getLocMemOffset();
  unsigned Offset = 0;
  uint32_t RemainingSize = Flags.getByValSize();
  unsigned ByValAlign = Flags.getByValAlign();

  // Copy the first 4 words of byval arg to registers A0 - A3.
  // FIXME: Use a stricter alignment if it enables better optimization in passes
  //        run later.
  for (; RemainingSize >= 4 && LocMemOffset < 4 * 4;
       Offset += 4, RemainingSize -= 4, LocMemOffset += 4) {
    SDValue LoadPtr = DAG.getNode(ISD::ADD, dl, MVT::i32, Arg,
                                  DAG.getConstant(Offset, MVT::i32));
    SDValue LoadVal = DAG.getLoad(MVT::i32, dl, Chain, LoadPtr,
                                  MachinePointerInfo(),
                                  false, false, std::min(ByValAlign,
                                                         (unsigned )4));
    MemOpChains.push_back(LoadVal.getValue(1));
    unsigned DstReg = O32IntRegs[LocMemOffset / 4];
    RegsToPass.push_back(std::make_pair(DstReg, LoadVal));
  }

  if (RemainingSize == 0)
    return;

  // If there still is a register available for argument passing, write the
  // remaining part of the structure to it using subword loads and shifts.
  if (LocMemOffset < 4 * 4) {
    assert(RemainingSize <= 3 && RemainingSize >= 1 &&
           "There must be one to three bytes remaining.");
    unsigned LoadSize = (RemainingSize == 3 ? 2 : RemainingSize);
    SDValue LoadPtr = DAG.getNode(ISD::ADD, dl, MVT::i32, Arg,
                                  DAG.getConstant(Offset, MVT::i32));
    unsigned Alignment = std::min(ByValAlign, (unsigned )4);
    SDValue LoadVal = DAG.getExtLoad(ISD::ZEXTLOAD, dl, MVT::i32, Chain,
                                     LoadPtr, MachinePointerInfo(),
                                     MVT::getIntegerVT(LoadSize * 8), false,
                                     false, Alignment);
    MemOpChains.push_back(LoadVal.getValue(1));

    // If target is big endian, shift it to the most significant half-word or
    // byte.
    if (!isLittle)
      LoadVal = DAG.getNode(ISD::SHL, dl, MVT::i32, LoadVal,
                            DAG.getConstant(32 - LoadSize * 8, MVT::i32));

    Offset += LoadSize;
    RemainingSize -= LoadSize;

    // Read second subword if necessary.
    if (RemainingSize != 0)  {
      assert(RemainingSize == 1 && "There must be one byte remaining.");
      LoadPtr = DAG.getNode(ISD::ADD, dl, MVT::i32, Arg, 
                            DAG.getConstant(Offset, MVT::i32));
      unsigned Alignment = std::min(ByValAlign, (unsigned )2);
      SDValue Subword = DAG.getExtLoad(ISD::ZEXTLOAD, dl, MVT::i32, Chain,
                                       LoadPtr, MachinePointerInfo(),
                                       MVT::i8, false, false, Alignment);
      MemOpChains.push_back(Subword.getValue(1));
      // Insert the loaded byte to LoadVal.
      // FIXME: Use INS if supported by target.
      unsigned ShiftAmt = isLittle ? 16 : 8;
      SDValue Shift = DAG.getNode(ISD::SHL, dl, MVT::i32, Subword,
                                  DAG.getConstant(ShiftAmt, MVT::i32));
      LoadVal = DAG.getNode(ISD::OR, dl, MVT::i32, LoadVal, Shift);
    }

    unsigned DstReg = O32IntRegs[LocMemOffset / 4];
    RegsToPass.push_back(std::make_pair(DstReg, LoadVal));
    return;
  }

  // Create a fixed object on stack at offset LocMemOffset and copy
  // remaining part of byval arg to it using memcpy.
  SDValue Src = DAG.getNode(ISD::ADD, dl, MVT::i32, Arg,
                            DAG.getConstant(Offset, MVT::i32));
  LastFI = MFI->CreateFixedObject(RemainingSize, LocMemOffset, true);
  SDValue Dst = DAG.getFrameIndex(LastFI, PtrType);
  ByValChain = DAG.getMemcpy(ByValChain, dl, Dst, Src,
                             DAG.getConstant(RemainingSize, MVT::i32),
                             std::min(ByValAlign, (unsigned)4),
                             /*isVolatile=*/false, /*AlwaysInline=*/false,
                             MachinePointerInfo(0), MachinePointerInfo(0));
}

/// LowerCall - functions arguments are copied from virtual regs to
/// (physical regs)/(stack frame), CALLSEQ_START and CALLSEQ_END are emitted.
/// TODO: isTailCall.
SDValue
MipsTargetLowering::LowerCall(SDValue InChain, SDValue Callee,
                              CallingConv::ID CallConv, bool isVarArg,
                              bool &isTailCall,
                              const SmallVectorImpl<ISD::OutputArg> &Outs,
                              const SmallVectorImpl<SDValue> &OutVals,
                              const SmallVectorImpl<ISD::InputArg> &Ins,
                              DebugLoc dl, SelectionDAG &DAG,
                              SmallVectorImpl<SDValue> &InVals) const {
  // MIPs target does not yet support tail call optimization.
  isTailCall = false;

  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  const TargetFrameLowering *TFL = MF.getTarget().getFrameLowering();
  bool IsPIC = getTargetMachine().getRelocationModel() == Reloc::PIC_;
  MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
		 getTargetMachine(), ArgLocs, *DAG.getContext());

  if (Subtarget->isABI_O32())
    CCInfo.AnalyzeCallOperands(Outs, CC_MipsO32);
  else
    CCInfo.AnalyzeCallOperands(Outs, CC_Mips);

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NextStackOffset = CCInfo.getNextStackOffset();

  // Chain is the output chain of the last Load/Store or CopyToReg node.
  // ByValChain is the output chain of the last Memcpy node created for copying
  // byval arguments to the stack.
  SDValue Chain, CallSeqStart, ByValChain;
  SDValue NextStackOffsetVal = DAG.getIntPtrConstant(NextStackOffset, true);
  Chain = CallSeqStart = DAG.getCALLSEQ_START(InChain, NextStackOffsetVal);
  ByValChain = InChain;

  // If this is the first call, create a stack frame object that points to
  // a location to which .cprestore saves $gp.
  if (IsPIC && !MipsFI->getGPFI())
    MipsFI->setGPFI(MFI->CreateFixedObject(4, 0, true));

  // Get the frame index of the stack frame object that points to the location
  // of dynamically allocated area on the stack.
  int DynAllocFI = MipsFI->getDynAllocFI();

  // Update size of the maximum argument space.
  // For O32, a minimum of four words (16 bytes) of argument space is
  // allocated.
  if (Subtarget->isABI_O32())
    NextStackOffset = std::max(NextStackOffset, (unsigned)16);

  unsigned MaxCallFrameSize = MipsFI->getMaxCallFrameSize();

  if (MaxCallFrameSize < NextStackOffset) {
    MipsFI->setMaxCallFrameSize(NextStackOffset);

    // Set the offsets relative to $sp of the $gp restore slot and dynamically
    // allocated stack space. These offsets must be aligned to a boundary
    // determined by the stack alignment of the ABI.
    unsigned StackAlignment = TFL->getStackAlignment();
    NextStackOffset = (NextStackOffset + StackAlignment - 1) /
                      StackAlignment * StackAlignment;

    if (IsPIC)
      MFI->setObjectOffset(MipsFI->getGPFI(), NextStackOffset);

    MFI->setObjectOffset(DynAllocFI, NextStackOffset);
  }

  // With EABI is it possible to have 16 args on registers.
  SmallVector<std::pair<unsigned, SDValue>, 16> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  int FirstFI = -MFI->getNumFixedObjects() - 1, LastFI = 0;

  // Walk the register/memloc assignments, inserting copies/loads.
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    SDValue Arg = OutVals[i];
    CCValAssign &VA = ArgLocs[i];

    // Promote the value if needed.
    switch (VA.getLocInfo()) {
    default: llvm_unreachable("Unknown loc info!");
    case CCValAssign::Full:
      if (Subtarget->isABI_O32() && VA.isRegLoc()) {
        if (VA.getValVT() == MVT::f32 && VA.getLocVT() == MVT::i32)
          Arg = DAG.getNode(ISD::BITCAST, dl, MVT::i32, Arg);
        if (VA.getValVT() == MVT::f64 && VA.getLocVT() == MVT::i32) {
          SDValue Lo = DAG.getNode(MipsISD::ExtractElementF64, dl, MVT::i32,
                                   Arg, DAG.getConstant(0, MVT::i32));
          SDValue Hi = DAG.getNode(MipsISD::ExtractElementF64, dl, MVT::i32,
                                   Arg, DAG.getConstant(1, MVT::i32));
          if (!Subtarget->isLittle())
            std::swap(Lo, Hi);
          unsigned LocRegLo = VA.getLocReg(); 
          unsigned LocRegHigh = getNextIntArgReg(LocRegLo);
          RegsToPass.push_back(std::make_pair(LocRegLo, Lo));
          RegsToPass.push_back(std::make_pair(LocRegHigh, Hi));
          continue;
        }
      }
      break;
    case CCValAssign::SExt:
      Arg = DAG.getNode(ISD::SIGN_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::ZExt:
      Arg = DAG.getNode(ISD::ZERO_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    case CCValAssign::AExt:
      Arg = DAG.getNode(ISD::ANY_EXTEND, dl, VA.getLocVT(), Arg);
      break;
    }

    // Arguments that can be passed on register must be kept at
    // RegsToPass vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    }

    // Register can't get to this point...
    assert(VA.isMemLoc());

    // ByVal Arg.
    ISD::ArgFlagsTy Flags = Outs[i].Flags;
    if (Flags.isByVal()) {
      assert(Subtarget->isABI_O32() &&
             "No support for ByVal args by ABIs other than O32 yet.");
      assert(Flags.getByValSize() &&
             "ByVal args of size 0 should have been ignored by front-end.");
      WriteByValArg(ByValChain, Chain, dl, RegsToPass, MemOpChains, LastFI, MFI,
                    DAG, Arg, VA, Flags, getPointerTy(), Subtarget->isLittle());
      continue;
    }

    // Create the frame index object for this incoming parameter
    LastFI = MFI->CreateFixedObject(VA.getValVT().getSizeInBits()/8,
                                    VA.getLocMemOffset(), true);
    SDValue PtrOff = DAG.getFrameIndex(LastFI, getPointerTy());

    // emit ISD::STORE whichs stores the
    // parameter value to a stack Location
    MemOpChains.push_back(DAG.getStore(Chain, dl, Arg, PtrOff,
                                       MachinePointerInfo(),
                                       false, false, 0));
  }

  // Extend range of indices of frame objects for outgoing arguments that were
  // created during this function call. Skip this step if no such objects were
  // created.
  if (LastFI)
    MipsFI->extendOutArgFIRange(FirstFI, LastFI);

  // If a memcpy has been created to copy a byval arg to a stack, replace the
  // chain input of CallSeqStart with ByValChain.
  if (InChain != ByValChain)
    DAG.UpdateNodeOperands(CallSeqStart.getNode(), ByValChain,
                           NextStackOffsetVal);

  // Transform all store nodes into one single node because all store
  // nodes are independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other,
                        &MemOpChains[0], MemOpChains.size());

  // If the callee is a GlobalAddress/ExternalSymbol node (quite common, every
  // direct call is) turn it into a TargetGlobalAddress/TargetExternalSymbol
  // node so that legalize doesn't hack it.
  unsigned char OpFlag = IsPIC ? MipsII::MO_GOT_CALL : MipsII::MO_NO_FLAG;
  bool LoadSymAddr = false;
  SDValue CalleeLo;

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    if (IsPIC && G->getGlobal()->hasInternalLinkage()) {
      Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl,
                                          getPointerTy(), 0,MipsII:: MO_GOT);
      CalleeLo = DAG.getTargetGlobalAddress(G->getGlobal(), dl, getPointerTy(),
                                            0, MipsII::MO_ABS_LO);
    } else {
      Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl,
                                          getPointerTy(), 0, OpFlag);
    }

    LoadSymAddr = true;
  }
  else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(),
                                getPointerTy(), OpFlag);
    LoadSymAddr = true;
  }

  SDValue InFlag;

  // Create nodes that load address of callee and copy it to T9
  if (IsPIC) {
    if (LoadSymAddr) {
      // Load callee address
      Callee = DAG.getNode(MipsISD::WrapperPIC, dl, MVT::i32, Callee);
      SDValue LoadValue = DAG.getLoad(MVT::i32, dl, DAG.getEntryNode(), Callee,
                                      MachinePointerInfo::getGOT(),
                                      false, false, 0);

      // Use GOT+LO if callee has internal linkage.
      if (CalleeLo.getNode()) {
        SDValue Lo = DAG.getNode(MipsISD::Lo, dl, MVT::i32, CalleeLo);
        Callee = DAG.getNode(ISD::ADD, dl, MVT::i32, LoadValue, Lo);
      } else
        Callee = LoadValue;
    }

    // copy to T9
    Chain = DAG.getCopyToReg(Chain, dl, Mips::T9, Callee, SDValue(0, 0));
    InFlag = Chain.getValue(1);
    Callee = DAG.getRegister(Mips::T9, MVT::i32);
  }

  // Build a sequence of copy-to-reg nodes chained together with token
  // chain and flag operands which copy the outgoing args into registers.
  // The InFlag in necessary since all emitted instructions must be
  // stuck together.
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i) {
    Chain = DAG.getCopyToReg(Chain, dl, RegsToPass[i].first,
                             RegsToPass[i].second, InFlag);
    InFlag = Chain.getValue(1);
  }

  // MipsJmpLink = #chain, #target_address, #opt_in_flags...
  //             = Chain, Callee, Reg#1, Reg#2, ...
  //
  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i)
    Ops.push_back(DAG.getRegister(RegsToPass[i].first,
                                  RegsToPass[i].second.getValueType()));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain  = DAG.getNode(MipsISD::JmpLink, dl, NodeTys, &Ops[0], Ops.size());
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain,
                             DAG.getIntPtrConstant(NextStackOffset, true),
                             DAG.getIntPtrConstant(0, true), InFlag);
  InFlag = Chain.getValue(1);

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return LowerCallResult(Chain, InFlag, CallConv, isVarArg,
                         Ins, dl, DAG, InVals);
}

/// LowerCallResult - Lower the result values of a call into the
/// appropriate copies out of appropriate physical registers.
SDValue
MipsTargetLowering::LowerCallResult(SDValue Chain, SDValue InFlag,
                                    CallingConv::ID CallConv, bool isVarArg,
                                    const SmallVectorImpl<ISD::InputArg> &Ins,
                                    DebugLoc dl, SelectionDAG &DAG,
                                    SmallVectorImpl<SDValue> &InVals) const {
  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
		 getTargetMachine(), RVLocs, *DAG.getContext());

  CCInfo.AnalyzeCallResult(Ins, RetCC_Mips);

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    Chain = DAG.getCopyFromReg(Chain, dl, RVLocs[i].getLocReg(),
                               RVLocs[i].getValVT(), InFlag).getValue(1);
    InFlag = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
//             Formal Arguments Calling Convention Implementation
//===----------------------------------------------------------------------===//
static void ReadByValArg(MachineFunction &MF, SDValue Chain, DebugLoc dl,
                         std::vector<SDValue>& OutChains,
                         SelectionDAG &DAG, unsigned NumWords, SDValue FIN,
                         const CCValAssign &VA, const ISD::ArgFlagsTy& Flags) {
  unsigned LocMem = VA.getLocMemOffset();
  unsigned FirstWord = LocMem / 4;

  // copy register A0 - A3 to frame object
  for (unsigned i = 0; i < NumWords; ++i) {
    unsigned CurWord = FirstWord + i;
    if (CurWord >= O32IntRegsSize)
      break;

    unsigned SrcReg = O32IntRegs[CurWord];
    unsigned Reg = AddLiveIn(MF, SrcReg, Mips::CPURegsRegisterClass);
    SDValue StorePtr = DAG.getNode(ISD::ADD, dl, MVT::i32, FIN,
                                   DAG.getConstant(i * 4, MVT::i32));
    SDValue Store = DAG.getStore(Chain, dl, DAG.getRegister(Reg, MVT::i32),
                                 StorePtr, MachinePointerInfo(), false,
                                 false, 0);
    OutChains.push_back(Store);
  }
}

/// LowerFormalArguments - transform physical registers into virtual registers
/// and generate load operations for arguments places on the stack.
SDValue
MipsTargetLowering::LowerFormalArguments(SDValue Chain,
                                         CallingConv::ID CallConv,
                                         bool isVarArg,
                                         const SmallVectorImpl<ISD::InputArg>
                                         &Ins,
                                         DebugLoc dl, SelectionDAG &DAG,
                                         SmallVectorImpl<SDValue> &InVals)
                                          const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();

  MipsFI->setVarArgsFrameIndex(0);

  // Used with vargs to acumulate store chains.
  std::vector<SDValue> OutChains;

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
		 getTargetMachine(), ArgLocs, *DAG.getContext());

  if (Subtarget->isABI_O32())
    CCInfo.AnalyzeFormalArguments(Ins, CC_MipsO32);
  else
    CCInfo.AnalyzeFormalArguments(Ins, CC_Mips);

  int LastFI = 0;// MipsFI->LastInArgFI is 0 at the entry of this function.

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    // Arguments stored on registers
    if (VA.isRegLoc()) {
      EVT RegVT = VA.getLocVT();
      unsigned ArgReg = VA.getLocReg();
      TargetRegisterClass *RC = 0;

      if (RegVT == MVT::i32)
        RC = Mips::CPURegsRegisterClass;
      else if (RegVT == MVT::i64)
        RC = Mips::CPU64RegsRegisterClass;
      else if (RegVT == MVT::f32)
        RC = Mips::FGR32RegisterClass;
      else if (RegVT == MVT::f64)
        RC = HasMips64 ? Mips::FGR64RegisterClass : Mips::AFGR64RegisterClass;
      else
        llvm_unreachable("RegVT not supported by FormalArguments Lowering");

      // Transform the arguments stored on
      // physical registers into virtual ones
      unsigned Reg = AddLiveIn(DAG.getMachineFunction(), ArgReg, RC);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, dl, Reg, RegVT);

      // If this is an 8 or 16-bit value, it has been passed promoted
      // to 32 bits.  Insert an assert[sz]ext to capture this, then
      // truncate to the right size.
      if (VA.getLocInfo() != CCValAssign::Full) {
        unsigned Opcode = 0;
        if (VA.getLocInfo() == CCValAssign::SExt)
          Opcode = ISD::AssertSext;
        else if (VA.getLocInfo() == CCValAssign::ZExt)
          Opcode = ISD::AssertZext;
        if (Opcode)
          ArgValue = DAG.getNode(Opcode, dl, RegVT, ArgValue,
                                 DAG.getValueType(VA.getValVT()));
        ArgValue = DAG.getNode(ISD::TRUNCATE, dl, VA.getValVT(), ArgValue);
      }

      // Handle O32 ABI cases: i32->f32 and (i32,i32)->f64
      if (Subtarget->isABI_O32()) {
        if (RegVT == MVT::i32 && VA.getValVT() == MVT::f32)
          ArgValue = DAG.getNode(ISD::BITCAST, dl, MVT::f32, ArgValue);
        if (RegVT == MVT::i32 && VA.getValVT() == MVT::f64) {
          unsigned Reg2 = AddLiveIn(DAG.getMachineFunction(),
                                    getNextIntArgReg(ArgReg), RC);
          SDValue ArgValue2 = DAG.getCopyFromReg(Chain, dl, Reg2, RegVT);
          if (!Subtarget->isLittle())
            std::swap(ArgValue, ArgValue2);
          ArgValue = DAG.getNode(MipsISD::BuildPairF64, dl, MVT::f64,
                                 ArgValue, ArgValue2);
        }
      }

      InVals.push_back(ArgValue);
    } else { // VA.isRegLoc()

      // sanity check
      assert(VA.isMemLoc());

      ISD::ArgFlagsTy Flags = Ins[i].Flags;

      if (Flags.isByVal()) {
        assert(Subtarget->isABI_O32() &&
               "No support for ByVal args by ABIs other than O32 yet.");
        assert(Flags.getByValSize() &&
               "ByVal args of size 0 should have been ignored by front-end.");
        unsigned NumWords = (Flags.getByValSize() + 3) / 4;
        LastFI = MFI->CreateFixedObject(NumWords * 4, VA.getLocMemOffset(),
                                        true);
        SDValue FIN = DAG.getFrameIndex(LastFI, getPointerTy());
        InVals.push_back(FIN);
        ReadByValArg(MF, Chain, dl, OutChains, DAG, NumWords, FIN, VA, Flags);

        continue;
      }

      // The stack pointer offset is relative to the caller stack frame.
      LastFI = MFI->CreateFixedObject(VA.getValVT().getSizeInBits()/8,
                                      VA.getLocMemOffset(), true);

      // Create load nodes to retrieve arguments from the stack
      SDValue FIN = DAG.getFrameIndex(LastFI, getPointerTy());
      InVals.push_back(DAG.getLoad(VA.getValVT(), dl, Chain, FIN,
                                   MachinePointerInfo::getFixedStack(LastFI),
                                   false, false, 0));
    }
  }

  // The mips ABIs for returning structs by value requires that we copy
  // the sret argument into $v0 for the return. Save the argument into
  // a virtual register so that we can access it from the return points.
  if (DAG.getMachineFunction().getFunction()->hasStructRetAttr()) {
    unsigned Reg = MipsFI->getSRetReturnReg();
    if (!Reg) {
      Reg = MF.getRegInfo().createVirtualRegister(getRegClassFor(MVT::i32));
      MipsFI->setSRetReturnReg(Reg);
    }
    SDValue Copy = DAG.getCopyToReg(DAG.getEntryNode(), dl, Reg, InVals[0]);
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, Copy, Chain);
  }

  if (isVarArg && Subtarget->isABI_O32()) {
    // Record the frame index of the first variable argument
    // which is a value necessary to VASTART.
    unsigned NextStackOffset = CCInfo.getNextStackOffset();
    assert(NextStackOffset % 4 == 0 &&
           "NextStackOffset must be aligned to 4-byte boundaries.");
    LastFI = MFI->CreateFixedObject(4, NextStackOffset, true);
    MipsFI->setVarArgsFrameIndex(LastFI);

    // If NextStackOffset is smaller than o32's 16-byte reserved argument area,
    // copy the integer registers that have not been used for argument passing
    // to the caller's stack frame.
    for (; NextStackOffset < 16; NextStackOffset += 4) {
      TargetRegisterClass *RC = Mips::CPURegsRegisterClass;
      unsigned Idx = NextStackOffset / 4;
      unsigned Reg = AddLiveIn(DAG.getMachineFunction(), O32IntRegs[Idx], RC);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, dl, Reg, MVT::i32);
      LastFI = MFI->CreateFixedObject(4, NextStackOffset, true);
      SDValue PtrOff = DAG.getFrameIndex(LastFI, getPointerTy());
      OutChains.push_back(DAG.getStore(Chain, dl, ArgValue, PtrOff,
                                       MachinePointerInfo(),
                                       false, false, 0));
    }
  }

  MipsFI->setLastInArgFI(LastFI);

  // All stores are grouped in one node to allow the matching between
  // the size of Ins and InVals. This only happens when on varg functions
  if (!OutChains.empty()) {
    OutChains.push_back(Chain);
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other,
                        &OutChains[0], OutChains.size());
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
//               Return Value Calling Convention Implementation
//===----------------------------------------------------------------------===//

SDValue
MipsTargetLowering::LowerReturn(SDValue Chain,
                                CallingConv::ID CallConv, bool isVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                DebugLoc dl, SelectionDAG &DAG) const {

  // CCValAssign - represent the assignment of
  // the return value to a location
  SmallVector<CCValAssign, 16> RVLocs;

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
		 getTargetMachine(), RVLocs, *DAG.getContext());

  // Analize return values.
  CCInfo.AnalyzeReturn(Outs, RetCC_Mips);

  // If this is the first return lowered for this function, add
  // the regs to the liveout set for the function.
  if (DAG.getMachineFunction().getRegInfo().liveout_empty()) {
    for (unsigned i = 0; i != RVLocs.size(); ++i)
      if (RVLocs[i].isRegLoc())
        DAG.getMachineFunction().getRegInfo().addLiveOut(RVLocs[i].getLocReg());
  }

  SDValue Flag;

  // Copy the result values into the output registers.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(),
                             OutVals[i], Flag);

    // guarantee that all emitted copies are
    // stuck together, avoiding something bad
    Flag = Chain.getValue(1);
  }

  // The mips ABIs for returning structs by value requires that we copy
  // the sret argument into $v0 for the return. We saved the argument into
  // a virtual register in the entry block, so now we copy the value out
  // and into $v0.
  if (DAG.getMachineFunction().getFunction()->hasStructRetAttr()) {
    MachineFunction &MF      = DAG.getMachineFunction();
    MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();
    unsigned Reg = MipsFI->getSRetReturnReg();

    if (!Reg)
      llvm_unreachable("sret virtual register not created in the entry block");
    SDValue Val = DAG.getCopyFromReg(Chain, dl, Reg, getPointerTy());

    Chain = DAG.getCopyToReg(Chain, dl, Mips::V0, Val, Flag);
    Flag = Chain.getValue(1);
  }

  // Return on Mips is always a "jr $ra"
  if (Flag.getNode())
    return DAG.getNode(MipsISD::Ret, dl, MVT::Other,
                       Chain, DAG.getRegister(Mips::RA, MVT::i32), Flag);
  else // Return Void
    return DAG.getNode(MipsISD::Ret, dl, MVT::Other,
                       Chain, DAG.getRegister(Mips::RA, MVT::i32));
}

//===----------------------------------------------------------------------===//
//                           Mips Inline Assembly Support
//===----------------------------------------------------------------------===//

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
MipsTargetLowering::ConstraintType MipsTargetLowering::
getConstraintType(const std::string &Constraint) const
{
  // Mips specific constrainy
  // GCC config/mips/constraints.md
  //
  // 'd' : An address register. Equivalent to r
  //       unless generating MIPS16 code.
  // 'y' : Equivalent to r; retained for
  //       backwards compatibility.
  // 'f' : Floating Point registers.
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
      default : break;
      case 'd':
      case 'y':
      case 'f':
        return C_RegisterClass;
        break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

/// Examine constraint type and operand type and determine a weight value.
/// This object must already have been set up with the operand type
/// and the current alternative constraint selected.
TargetLowering::ConstraintWeight
MipsTargetLowering::getSingleConstraintMatchWeight(
    AsmOperandInfo &info, const char *constraint) const {
  ConstraintWeight weight = CW_Invalid;
  Value *CallOperandVal = info.CallOperandVal;
    // If we don't have a value, we can't do a match,
    // but allow it at the lowest weight.
  if (CallOperandVal == NULL)
    return CW_Default;
  Type *type = CallOperandVal->getType();
  // Look at the constraint type.
  switch (*constraint) {
  default:
    weight = TargetLowering::getSingleConstraintMatchWeight(info, constraint);
    break;
  case 'd':
  case 'y':
    if (type->isIntegerTy())
      weight = CW_Register;
    break;
  case 'f':
    if (type->isFloatTy())
      weight = CW_Register;
    break;
  }
  return weight;
}

/// Given a register class constraint, like 'r', if this corresponds directly
/// to an LLVM register class, return a register of 0 and the register class
/// pointer.
std::pair<unsigned, const TargetRegisterClass*> MipsTargetLowering::
getRegForInlineAsmConstraint(const std::string &Constraint, EVT VT) const
{
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'd': // Address register. Same as 'r' unless generating MIPS16 code.
    case 'y': // Same as 'r'. Exists for compatibility.
    case 'r':
      return std::make_pair(0U, Mips::CPURegsRegisterClass);
    case 'f':
      if (VT == MVT::f32)
        return std::make_pair(0U, Mips::FGR32RegisterClass);
      if (VT == MVT::f64)
        if ((!Subtarget->isSingleFloat()) && (!Subtarget->isFP64bit()))
          return std::make_pair(0U, Mips::AFGR64RegisterClass);
      break;
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(Constraint, VT);
}

bool
MipsTargetLowering::isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
  // The Mips target isn't yet aware of offsets.
  return false;
}

bool MipsTargetLowering::isFPImmLegal(const APFloat &Imm, EVT VT) const {
  if (VT != MVT::f32 && VT != MVT::f64)
    return false;
  if (Imm.isNegZero())
    return false;
  return Imm.isZero();
}
