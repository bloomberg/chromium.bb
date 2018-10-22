//==--- InstrEmitter.cpp - Emit MachineInstrs for the SelectionDAG class ---==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This implements the Emit routines for the SelectionDAG class, which creates
// MachineInstrs based on the decisions of the SelectionDAG instruction
// selection.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "instr-emitter"
#include "InstrEmitter.h"
#include "SDNodeDbgValue.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
using namespace llvm;

/// MinRCSize - Smallest register class we allow when constraining virtual
/// registers.  If satisfying all register class constraints would require
/// using a smaller register class, emit a COPY to a new virtual register
/// instead.
const unsigned MinRCSize = 4;

/// CountResults - The results of target nodes have register or immediate
/// operands first, then an optional chain, and optional glue operands (which do
/// not go into the resulting MachineInstr).
unsigned InstrEmitter::CountResults(SDNode *Node) {
  unsigned N = Node->getNumValues();
  while (N && Node->getValueType(N - 1) == MVT::Glue)
    --N;
  if (N && Node->getValueType(N - 1) == MVT::Other)
    --N;    // Skip over chain result.
  return N;
}

/// CountOperands - The inputs to target nodes have any actual inputs first,
/// followed by an optional chain operand, then an optional glue operand.
/// Compute the number of actual operands that will go into the resulting
/// MachineInstr.
unsigned InstrEmitter::CountOperands(SDNode *Node) {
  unsigned N = Node->getNumOperands();
  while (N && Node->getOperand(N - 1).getValueType() == MVT::Glue)
    --N;
  if (N && Node->getOperand(N - 1).getValueType() == MVT::Other)
    --N; // Ignore chain if it exists.
  return N;
}

/// EmitCopyFromReg - Generate machine code for an CopyFromReg node or an
/// implicit physical register output.
void InstrEmitter::
EmitCopyFromReg(SDNode *Node, unsigned ResNo, bool IsClone, bool IsCloned,
                unsigned SrcReg, DenseMap<SDValue, unsigned> &VRBaseMap) {
  unsigned VRBase = 0;
  if (TargetRegisterInfo::isVirtualRegister(SrcReg)) {
    // Just use the input register directly!
    SDValue Op(Node, ResNo);
    if (IsClone)
      VRBaseMap.erase(Op);
    bool isNew = VRBaseMap.insert(std::make_pair(Op, SrcReg)).second;
    (void)isNew; // Silence compiler warning.
    assert(isNew && "Node emitted out of order - early");
    return;
  }

  // If the node is only used by a CopyToReg and the dest reg is a vreg, use
  // the CopyToReg'd destination register instead of creating a new vreg.
  bool MatchReg = true;
  const TargetRegisterClass *UseRC = NULL;
  EVT VT = Node->getValueType(ResNo);

  // Stick to the preferred register classes for legal types.
  if (TLI->isTypeLegal(VT))
    UseRC = TLI->getRegClassFor(VT);

  if (!IsClone && !IsCloned)
    for (SDNode::use_iterator UI = Node->use_begin(), E = Node->use_end();
         UI != E; ++UI) {
      SDNode *User = *UI;
      bool Match = true;
      if (User->getOpcode() == ISD::CopyToReg &&
          User->getOperand(2).getNode() == Node &&
          User->getOperand(2).getResNo() == ResNo) {
        unsigned DestReg = cast<RegisterSDNode>(User->getOperand(1))->getReg();
        if (TargetRegisterInfo::isVirtualRegister(DestReg)) {
          VRBase = DestReg;
          Match = false;
        } else if (DestReg != SrcReg)
          Match = false;
      } else {
        for (unsigned i = 0, e = User->getNumOperands(); i != e; ++i) {
          SDValue Op = User->getOperand(i);
          if (Op.getNode() != Node || Op.getResNo() != ResNo)
            continue;
          EVT VT = Node->getValueType(Op.getResNo());
          if (VT == MVT::Other || VT == MVT::Glue)
            continue;
          Match = false;
          if (User->isMachineOpcode()) {
            const MCInstrDesc &II = TII->get(User->getMachineOpcode());
            const TargetRegisterClass *RC = 0;
            if (i+II.getNumDefs() < II.getNumOperands())
              RC = TII->getRegClass(II, i+II.getNumDefs(), TRI);
            if (!UseRC)
              UseRC = RC;
            else if (RC) {
              const TargetRegisterClass *ComRC =
                TRI->getCommonSubClass(UseRC, RC);
              // If multiple uses expect disjoint register classes, we emit
              // copies in AddRegisterOperand.
              if (ComRC)
                UseRC = ComRC;
            }
          }
        }
      }
      MatchReg &= Match;
      if (VRBase)
        break;
    }

  const TargetRegisterClass *SrcRC = 0, *DstRC = 0;
  SrcRC = TRI->getMinimalPhysRegClass(SrcReg, VT);

  // Figure out the register class to create for the destreg.
  if (VRBase) {
    DstRC = MRI->getRegClass(VRBase);
  } else if (UseRC) {
    assert(UseRC->hasType(VT) && "Incompatible phys register def and uses!");
    DstRC = UseRC;
  } else {
    DstRC = TLI->getRegClassFor(VT);
  }

  // If all uses are reading from the src physical register and copying the
  // register is either impossible or very expensive, then don't create a copy.
  if (MatchReg && SrcRC->getCopyCost() < 0) {
    VRBase = SrcReg;
  } else {
    // Create the reg, emit the copy.
    VRBase = MRI->createVirtualRegister(DstRC);
    BuildMI(*MBB, InsertPos, Node->getDebugLoc(), TII->get(TargetOpcode::COPY),
            VRBase).addReg(SrcReg);
  }

  SDValue Op(Node, ResNo);
  if (IsClone)
    VRBaseMap.erase(Op);
  bool isNew = VRBaseMap.insert(std::make_pair(Op, VRBase)).second;
  (void)isNew; // Silence compiler warning.
  assert(isNew && "Node emitted out of order - early");
}

/// getDstOfCopyToRegUse - If the only use of the specified result number of
/// node is a CopyToReg, return its destination register. Return 0 otherwise.
unsigned InstrEmitter::getDstOfOnlyCopyToRegUse(SDNode *Node,
                                                unsigned ResNo) const {
  if (!Node->hasOneUse())
    return 0;

  SDNode *User = *Node->use_begin();
  if (User->getOpcode() == ISD::CopyToReg &&
      User->getOperand(2).getNode() == Node &&
      User->getOperand(2).getResNo() == ResNo) {
    unsigned Reg = cast<RegisterSDNode>(User->getOperand(1))->getReg();
    if (TargetRegisterInfo::isVirtualRegister(Reg))
      return Reg;
  }
  return 0;
}

void InstrEmitter::CreateVirtualRegisters(SDNode *Node, MachineInstr *MI,
                                       const MCInstrDesc &II,
                                       bool IsClone, bool IsCloned,
                                       DenseMap<SDValue, unsigned> &VRBaseMap) {
  assert(Node->getMachineOpcode() != TargetOpcode::IMPLICIT_DEF &&
         "IMPLICIT_DEF should have been handled as a special case elsewhere!");

  for (unsigned i = 0; i < II.getNumDefs(); ++i) {
    // If the specific node value is only used by a CopyToReg and the dest reg
    // is a vreg in the same register class, use the CopyToReg'd destination
    // register instead of creating a new vreg.
    unsigned VRBase = 0;
    const TargetRegisterClass *RC = TII->getRegClass(II, i, TRI);
    if (II.OpInfo[i].isOptionalDef()) {
      // Optional def must be a physical register.
      unsigned NumResults = CountResults(Node);
      VRBase = cast<RegisterSDNode>(Node->getOperand(i-NumResults))->getReg();
      assert(TargetRegisterInfo::isPhysicalRegister(VRBase));
      MI->addOperand(MachineOperand::CreateReg(VRBase, true));
    }

    if (!VRBase && !IsClone && !IsCloned)
      for (SDNode::use_iterator UI = Node->use_begin(), E = Node->use_end();
           UI != E; ++UI) {
        SDNode *User = *UI;
        if (User->getOpcode() == ISD::CopyToReg &&
            User->getOperand(2).getNode() == Node &&
            User->getOperand(2).getResNo() == i) {
          unsigned Reg = cast<RegisterSDNode>(User->getOperand(1))->getReg();
          if (TargetRegisterInfo::isVirtualRegister(Reg)) {
            const TargetRegisterClass *RegRC = MRI->getRegClass(Reg);
            if (RegRC == RC) {
              VRBase = Reg;
              MI->addOperand(MachineOperand::CreateReg(Reg, true));
              break;
            }
          }
        }
      }

    // Create the result registers for this node and add the result regs to
    // the machine instruction.
    if (VRBase == 0) {
      assert(RC && "Isn't a register operand!");
      VRBase = MRI->createVirtualRegister(RC);
      MI->addOperand(MachineOperand::CreateReg(VRBase, true));
    }

    SDValue Op(Node, i);
    if (IsClone)
      VRBaseMap.erase(Op);
    bool isNew = VRBaseMap.insert(std::make_pair(Op, VRBase)).second;
    (void)isNew; // Silence compiler warning.
    assert(isNew && "Node emitted out of order - early");
  }
}

/// getVR - Return the virtual register corresponding to the specified result
/// of the specified node.
unsigned InstrEmitter::getVR(SDValue Op,
                             DenseMap<SDValue, unsigned> &VRBaseMap) {
  if (Op.isMachineOpcode() &&
      Op.getMachineOpcode() == TargetOpcode::IMPLICIT_DEF) {
    // Add an IMPLICIT_DEF instruction before every use.
    unsigned VReg = getDstOfOnlyCopyToRegUse(Op.getNode(), Op.getResNo());
    // IMPLICIT_DEF can produce any type of result so its MCInstrDesc
    // does not include operand register class info.
    if (!VReg) {
      const TargetRegisterClass *RC = TLI->getRegClassFor(Op.getValueType());
      VReg = MRI->createVirtualRegister(RC);
    }
    BuildMI(*MBB, InsertPos, Op.getDebugLoc(),
            TII->get(TargetOpcode::IMPLICIT_DEF), VReg);
    return VReg;
  }

  DenseMap<SDValue, unsigned>::iterator I = VRBaseMap.find(Op);
  assert(I != VRBaseMap.end() && "Node emitted out of order - late");
  return I->second;
}


/// AddRegisterOperand - Add the specified register as an operand to the
/// specified machine instr. Insert register copies if the register is
/// not in the required register class.
void
InstrEmitter::AddRegisterOperand(MachineInstr *MI, SDValue Op,
                                 unsigned IIOpNum,
                                 const MCInstrDesc *II,
                                 DenseMap<SDValue, unsigned> &VRBaseMap,
                                 bool IsDebug, bool IsClone, bool IsCloned) {
  assert(Op.getValueType() != MVT::Other &&
         Op.getValueType() != MVT::Glue &&
         "Chain and glue operands should occur at end of operand list!");
  // Get/emit the operand.
  unsigned VReg = getVR(Op, VRBaseMap);
  assert(TargetRegisterInfo::isVirtualRegister(VReg) && "Not a vreg?");

  const MCInstrDesc &MCID = MI->getDesc();
  bool isOptDef = IIOpNum < MCID.getNumOperands() &&
    MCID.OpInfo[IIOpNum].isOptionalDef();

  // If the instruction requires a register in a different class, create
  // a new virtual register and copy the value into it, but first attempt to
  // shrink VReg's register class within reason.  For example, if VReg == GR32
  // and II requires a GR32_NOSP, just constrain VReg to GR32_NOSP.
  if (II) {
    const TargetRegisterClass *DstRC = 0;
    if (IIOpNum < II->getNumOperands())
      DstRC = TII->getRegClass(*II, IIOpNum, TRI);
    assert((DstRC || (MCID.isVariadic() && IIOpNum >= MCID.getNumOperands())) &&
           "Don't have operand info for this instruction!");
    if (DstRC && !MRI->constrainRegClass(VReg, DstRC, MinRCSize)) {
      unsigned NewVReg = MRI->createVirtualRegister(DstRC);
      BuildMI(*MBB, InsertPos, Op.getNode()->getDebugLoc(),
              TII->get(TargetOpcode::COPY), NewVReg).addReg(VReg);
      VReg = NewVReg;
    }
  }

  // If this value has only one use, that use is a kill. This is a
  // conservative approximation. InstrEmitter does trivial coalescing
  // with CopyFromReg nodes, so don't emit kill flags for them.
  // Avoid kill flags on Schedule cloned nodes, since there will be
  // multiple uses.
  // Tied operands are never killed, so we need to check that. And that
  // means we need to determine the index of the operand.
  bool isKill = Op.hasOneUse() &&
                Op.getNode()->getOpcode() != ISD::CopyFromReg &&
                !IsDebug &&
                !(IsClone || IsCloned);
  if (isKill) {
    unsigned Idx = MI->getNumOperands();
    while (Idx > 0 &&
           MI->getOperand(Idx-1).isReg() && MI->getOperand(Idx-1).isImplicit())
      --Idx;
    bool isTied = MI->getDesc().getOperandConstraint(Idx, MCOI::TIED_TO) != -1;
    if (isTied)
      isKill = false;
  }

  MI->addOperand(MachineOperand::CreateReg(VReg, isOptDef,
                                           false/*isImp*/, isKill,
                                           false/*isDead*/, false/*isUndef*/,
                                           false/*isEarlyClobber*/,
                                           0/*SubReg*/, IsDebug));
}

/// AddOperand - Add the specified operand to the specified machine instr.  II
/// specifies the instruction information for the node, and IIOpNum is the
/// operand number (in the II) that we are adding. IIOpNum and II are used for
/// assertions only.
void InstrEmitter::AddOperand(MachineInstr *MI, SDValue Op,
                              unsigned IIOpNum,
                              const MCInstrDesc *II,
                              DenseMap<SDValue, unsigned> &VRBaseMap,
                              bool IsDebug, bool IsClone, bool IsCloned) {
  if (Op.isMachineOpcode()) {
    AddRegisterOperand(MI, Op, IIOpNum, II, VRBaseMap,
                       IsDebug, IsClone, IsCloned);
  } else if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateImm(C->getSExtValue()));
  } else if (ConstantFPSDNode *F = dyn_cast<ConstantFPSDNode>(Op)) {
    const ConstantFP *CFP = F->getConstantFPValue();
    MI->addOperand(MachineOperand::CreateFPImm(CFP));
  } else if (RegisterSDNode *R = dyn_cast<RegisterSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateReg(R->getReg(), false));
  } else if (GlobalAddressSDNode *TGA = dyn_cast<GlobalAddressSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateGA(TGA->getGlobal(), TGA->getOffset(),
                                            TGA->getTargetFlags()));
  } else if (BasicBlockSDNode *BBNode = dyn_cast<BasicBlockSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateMBB(BBNode->getBasicBlock()));
  } else if (FrameIndexSDNode *FI = dyn_cast<FrameIndexSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateFI(FI->getIndex()));
  } else if (JumpTableSDNode *JT = dyn_cast<JumpTableSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateJTI(JT->getIndex(),
                                             JT->getTargetFlags()));
  } else if (ConstantPoolSDNode *CP = dyn_cast<ConstantPoolSDNode>(Op)) {
    int Offset = CP->getOffset();
    unsigned Align = CP->getAlignment();
    Type *Type = CP->getType();
    // MachineConstantPool wants an explicit alignment.
    if (Align == 0) {
      Align = TM->getTargetData()->getPrefTypeAlignment(Type);
      if (Align == 0) {
        // Alignment of vector types.  FIXME!
        Align = TM->getTargetData()->getTypeAllocSize(Type);
      }
    }

    unsigned Idx;
    MachineConstantPool *MCP = MF->getConstantPool();
    if (CP->isMachineConstantPoolEntry())
      Idx = MCP->getConstantPoolIndex(CP->getMachineCPVal(), Align);
    else
      Idx = MCP->getConstantPoolIndex(CP->getConstVal(), Align);
    MI->addOperand(MachineOperand::CreateCPI(Idx, Offset,
                                             CP->getTargetFlags()));
  } else if (ExternalSymbolSDNode *ES = dyn_cast<ExternalSymbolSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateES(ES->getSymbol(),
                                            ES->getTargetFlags()));
  } else if (BlockAddressSDNode *BA = dyn_cast<BlockAddressSDNode>(Op)) {
    MI->addOperand(MachineOperand::CreateBA(BA->getBlockAddress(),
                                            BA->getTargetFlags()));
  } else {
    assert(Op.getValueType() != MVT::Other &&
           Op.getValueType() != MVT::Glue &&
           "Chain and glue operands should occur at end of operand list!");
    AddRegisterOperand(MI, Op, IIOpNum, II, VRBaseMap,
                       IsDebug, IsClone, IsCloned);
  }
}

unsigned InstrEmitter::ConstrainForSubReg(unsigned VReg, unsigned SubIdx,
                                          EVT VT, DebugLoc DL) {
  const TargetRegisterClass *VRC = MRI->getRegClass(VReg);
  const TargetRegisterClass *RC = TRI->getSubClassWithSubReg(VRC, SubIdx);

  // RC is a sub-class of VRC that supports SubIdx.  Try to constrain VReg
  // within reason.
  if (RC && RC != VRC)
    RC = MRI->constrainRegClass(VReg, RC, MinRCSize);

  // VReg has been adjusted.  It can be used with SubIdx operands now.
  if (RC)
    return VReg;

  // VReg couldn't be reasonably constrained.  Emit a COPY to a new virtual
  // register instead.
  RC = TRI->getSubClassWithSubReg(TLI->getRegClassFor(VT), SubIdx);
  assert(RC && "No legal register class for VT supports that SubIdx");
  unsigned NewReg = MRI->createVirtualRegister(RC);
  BuildMI(*MBB, InsertPos, DL, TII->get(TargetOpcode::COPY), NewReg)
    .addReg(VReg);
  return NewReg;
}

/// EmitSubregNode - Generate machine code for subreg nodes.
///
void InstrEmitter::EmitSubregNode(SDNode *Node,
                                  DenseMap<SDValue, unsigned> &VRBaseMap,
                                  bool IsClone, bool IsCloned) {
  unsigned VRBase = 0;
  unsigned Opc = Node->getMachineOpcode();

  // If the node is only used by a CopyToReg and the dest reg is a vreg, use
  // the CopyToReg'd destination register instead of creating a new vreg.
  for (SDNode::use_iterator UI = Node->use_begin(), E = Node->use_end();
       UI != E; ++UI) {
    SDNode *User = *UI;
    if (User->getOpcode() == ISD::CopyToReg &&
        User->getOperand(2).getNode() == Node) {
      unsigned DestReg = cast<RegisterSDNode>(User->getOperand(1))->getReg();
      if (TargetRegisterInfo::isVirtualRegister(DestReg)) {
        VRBase = DestReg;
        break;
      }
    }
  }

  if (Opc == TargetOpcode::EXTRACT_SUBREG) {
    // EXTRACT_SUBREG is lowered as %dst = COPY %src:sub.  There are no
    // constraints on the %dst register, COPY can target all legal register
    // classes.
    unsigned SubIdx = cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue();
    const TargetRegisterClass *TRC = TLI->getRegClassFor(Node->getValueType(0));

    unsigned VReg = getVR(Node->getOperand(0), VRBaseMap);
    MachineInstr *DefMI = MRI->getVRegDef(VReg);
    unsigned SrcReg, DstReg, DefSubIdx;
    if (DefMI &&
        TII->isCoalescableExtInstr(*DefMI, SrcReg, DstReg, DefSubIdx) &&
        SubIdx == DefSubIdx) {
      // Optimize these:
      // r1025 = s/zext r1024, 4
      // r1026 = extract_subreg r1025, 4
      // to a copy
      // r1026 = copy r1024
      VRBase = MRI->createVirtualRegister(TRC);
      BuildMI(*MBB, InsertPos, Node->getDebugLoc(),
              TII->get(TargetOpcode::COPY), VRBase).addReg(SrcReg);
    } else {
      // VReg may not support a SubIdx sub-register, and we may need to
      // constrain its register class or issue a COPY to a compatible register
      // class.
      VReg = ConstrainForSubReg(VReg, SubIdx,
                                Node->getOperand(0).getValueType(),
                                Node->getDebugLoc());

      // Create the destreg if it is missing.
      if (VRBase == 0)
        VRBase = MRI->createVirtualRegister(TRC);

      // Create the extract_subreg machine instruction.
      BuildMI(*MBB, InsertPos, Node->getDebugLoc(),
              TII->get(TargetOpcode::COPY), VRBase).addReg(VReg, 0, SubIdx);
    }
  } else if (Opc == TargetOpcode::INSERT_SUBREG ||
             Opc == TargetOpcode::SUBREG_TO_REG) {
    SDValue N0 = Node->getOperand(0);
    SDValue N1 = Node->getOperand(1);
    SDValue N2 = Node->getOperand(2);
    unsigned SubIdx = cast<ConstantSDNode>(N2)->getZExtValue();

    // Figure out the register class to create for the destreg.  It should be
    // the largest legal register class supporting SubIdx sub-registers.
    // RegisterCoalescer will constrain it further if it decides to eliminate
    // the INSERT_SUBREG instruction.
    //
    //   %dst = INSERT_SUBREG %src, %sub, SubIdx
    //
    // is lowered by TwoAddressInstructionPass to:
    //
    //   %dst = COPY %src
    //   %dst:SubIdx = COPY %sub
    //
    // There is no constraint on the %src register class.
    //
    const TargetRegisterClass *SRC = TLI->getRegClassFor(Node->getValueType(0));
    SRC = TRI->getSubClassWithSubReg(SRC, SubIdx);
    assert(SRC && "No register class supports VT and SubIdx for INSERT_SUBREG");

    if (VRBase == 0 || !SRC->hasSubClassEq(MRI->getRegClass(VRBase)))
      VRBase = MRI->createVirtualRegister(SRC);

    // Create the insert_subreg or subreg_to_reg machine instruction.
    MachineInstr *MI = BuildMI(*MF, Node->getDebugLoc(), TII->get(Opc));
    MI->addOperand(MachineOperand::CreateReg(VRBase, true));

    // If creating a subreg_to_reg, then the first input operand
    // is an implicit value immediate, otherwise it's a register
    if (Opc == TargetOpcode::SUBREG_TO_REG) {
      const ConstantSDNode *SD = cast<ConstantSDNode>(N0);
      MI->addOperand(MachineOperand::CreateImm(SD->getZExtValue()));
    } else
      AddOperand(MI, N0, 0, 0, VRBaseMap, /*IsDebug=*/false,
                 IsClone, IsCloned);
    // Add the subregster being inserted
    AddOperand(MI, N1, 0, 0, VRBaseMap, /*IsDebug=*/false,
               IsClone, IsCloned);
    MI->addOperand(MachineOperand::CreateImm(SubIdx));
    MBB->insert(InsertPos, MI);
  } else
    llvm_unreachable("Node is not insert_subreg, extract_subreg, or subreg_to_reg");

  SDValue Op(Node, 0);
  bool isNew = VRBaseMap.insert(std::make_pair(Op, VRBase)).second;
  (void)isNew; // Silence compiler warning.
  assert(isNew && "Node emitted out of order - early");
}

/// EmitCopyToRegClassNode - Generate machine code for COPY_TO_REGCLASS nodes.
/// COPY_TO_REGCLASS is just a normal copy, except that the destination
/// register is constrained to be in a particular register class.
///
void
InstrEmitter::EmitCopyToRegClassNode(SDNode *Node,
                                     DenseMap<SDValue, unsigned> &VRBaseMap) {
  unsigned VReg = getVR(Node->getOperand(0), VRBaseMap);

  // Create the new VReg in the destination class and emit a copy.
  unsigned DstRCIdx = cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue();
  const TargetRegisterClass *DstRC = TRI->getRegClass(DstRCIdx);
  unsigned NewVReg = MRI->createVirtualRegister(DstRC);
  BuildMI(*MBB, InsertPos, Node->getDebugLoc(), TII->get(TargetOpcode::COPY),
    NewVReg).addReg(VReg);

  SDValue Op(Node, 0);
  bool isNew = VRBaseMap.insert(std::make_pair(Op, NewVReg)).second;
  (void)isNew; // Silence compiler warning.
  assert(isNew && "Node emitted out of order - early");
}

/// EmitRegSequence - Generate machine code for REG_SEQUENCE nodes.
///
void InstrEmitter::EmitRegSequence(SDNode *Node,
                                  DenseMap<SDValue, unsigned> &VRBaseMap,
                                  bool IsClone, bool IsCloned) {
  unsigned DstRCIdx = cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue();
  const TargetRegisterClass *RC = TRI->getRegClass(DstRCIdx);
  unsigned NewVReg = MRI->createVirtualRegister(RC);
  MachineInstr *MI = BuildMI(*MF, Node->getDebugLoc(),
                             TII->get(TargetOpcode::REG_SEQUENCE), NewVReg);
  unsigned NumOps = Node->getNumOperands();
  assert((NumOps & 1) == 1 &&
         "REG_SEQUENCE must have an odd number of operands!");
  const MCInstrDesc &II = TII->get(TargetOpcode::REG_SEQUENCE);
  for (unsigned i = 1; i != NumOps; ++i) {
    SDValue Op = Node->getOperand(i);
    if ((i & 1) == 0) {
      unsigned SubIdx = cast<ConstantSDNode>(Op)->getZExtValue();
      unsigned SubReg = getVR(Node->getOperand(i-1), VRBaseMap);
      const TargetRegisterClass *TRC = MRI->getRegClass(SubReg);
      const TargetRegisterClass *SRC =
        TRI->getMatchingSuperRegClass(RC, TRC, SubIdx);
      if (SRC && SRC != RC) {
        MRI->setRegClass(NewVReg, SRC);
        RC = SRC;
      }
    }
    AddOperand(MI, Op, i+1, &II, VRBaseMap, /*IsDebug=*/false,
               IsClone, IsCloned);
  }

  MBB->insert(InsertPos, MI);
  SDValue Op(Node, 0);
  bool isNew = VRBaseMap.insert(std::make_pair(Op, NewVReg)).second;
  (void)isNew; // Silence compiler warning.
  assert(isNew && "Node emitted out of order - early");
}

/// EmitDbgValue - Generate machine instruction for a dbg_value node.
///
MachineInstr *
InstrEmitter::EmitDbgValue(SDDbgValue *SD,
                           DenseMap<SDValue, unsigned> &VRBaseMap) {
  uint64_t Offset = SD->getOffset();
  MDNode* MDPtr = SD->getMDPtr();
  DebugLoc DL = SD->getDebugLoc();

  if (SD->getKind() == SDDbgValue::FRAMEIX) {
    // Stack address; this needs to be lowered in target-dependent fashion.
    // EmitTargetCodeForFrameDebugValue is responsible for allocation.
    unsigned FrameIx = SD->getFrameIx();
    return TII->emitFrameIndexDebugValue(*MF, FrameIx, Offset, MDPtr, DL);
  }
  // Otherwise, we're going to create an instruction here.
  const MCInstrDesc &II = TII->get(TargetOpcode::DBG_VALUE);
  MachineInstrBuilder MIB = BuildMI(*MF, DL, II);
  if (SD->getKind() == SDDbgValue::SDNODE) {
    SDNode *Node = SD->getSDNode();
    SDValue Op = SDValue(Node, SD->getResNo());
    // It's possible we replaced this SDNode with other(s) and therefore
    // didn't generate code for it.  It's better to catch these cases where
    // they happen and transfer the debug info, but trying to guarantee that
    // in all cases would be very fragile; this is a safeguard for any
    // that were missed.
    DenseMap<SDValue, unsigned>::iterator I = VRBaseMap.find(Op);
    if (I==VRBaseMap.end())
      MIB.addReg(0U);       // undef
    else
      AddOperand(&*MIB, Op, (*MIB).getNumOperands(), &II, VRBaseMap,
                 /*IsDebug=*/true, /*IsClone=*/false, /*IsCloned=*/false);
  } else if (SD->getKind() == SDDbgValue::CONST) {
    const Value *V = SD->getConst();
    if (const ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
      if (CI->getBitWidth() > 64)
        MIB.addCImm(CI);
      else
        MIB.addImm(CI->getSExtValue());
    } else if (const ConstantFP *CF = dyn_cast<ConstantFP>(V)) {
      MIB.addFPImm(CF);
    } else {
      // Could be an Undef.  In any case insert an Undef so we can see what we
      // dropped.
      MIB.addReg(0U);
    }
  } else {
    // Insert an Undef so we can see what we dropped.
    MIB.addReg(0U);
  }

  MIB.addImm(Offset).addMetadata(MDPtr);
  return &*MIB;
}

/// EmitMachineNode - Generate machine code for a target-specific node and
/// needed dependencies.
///
void InstrEmitter::
EmitMachineNode(SDNode *Node, bool IsClone, bool IsCloned,
                DenseMap<SDValue, unsigned> &VRBaseMap) {
  unsigned Opc = Node->getMachineOpcode();

  // Handle subreg insert/extract specially
  if (Opc == TargetOpcode::EXTRACT_SUBREG ||
      Opc == TargetOpcode::INSERT_SUBREG ||
      Opc == TargetOpcode::SUBREG_TO_REG) {
    EmitSubregNode(Node, VRBaseMap, IsClone, IsCloned);
    return;
  }

  // Handle COPY_TO_REGCLASS specially.
  if (Opc == TargetOpcode::COPY_TO_REGCLASS) {
    EmitCopyToRegClassNode(Node, VRBaseMap);
    return;
  }

  // Handle REG_SEQUENCE specially.
  if (Opc == TargetOpcode::REG_SEQUENCE) {
    EmitRegSequence(Node, VRBaseMap, IsClone, IsCloned);
    return;
  }

  if (Opc == TargetOpcode::IMPLICIT_DEF)
    // We want a unique VR for each IMPLICIT_DEF use.
    return;

  const MCInstrDesc &II = TII->get(Opc);
  unsigned NumResults = CountResults(Node);
  unsigned NodeOperands = CountOperands(Node);
  bool HasPhysRegOuts = NumResults > II.getNumDefs() && II.getImplicitDefs()!=0;
#ifndef NDEBUG
  unsigned NumMIOperands = NodeOperands + NumResults;
  if (II.isVariadic())
    assert(NumMIOperands >= II.getNumOperands() &&
           "Too few operands for a variadic node!");
  else
    assert(NumMIOperands >= II.getNumOperands() &&
           NumMIOperands <= II.getNumOperands()+II.getNumImplicitDefs() &&
           "#operands for dag node doesn't match .td file!");
#endif

  // Create the new machine instruction.
  MachineInstr *MI = BuildMI(*MF, Node->getDebugLoc(), II);

  // The MachineInstr constructor adds implicit-def operands. Scan through
  // these to determine which are dead.
  if (MI->getNumOperands() != 0 &&
      Node->getValueType(Node->getNumValues()-1) == MVT::Glue) {
    // First, collect all used registers.
    SmallVector<unsigned, 8> UsedRegs;
    for (SDNode *F = Node->getGluedUser(); F; F = F->getGluedUser())
      if (F->getOpcode() == ISD::CopyFromReg)
        UsedRegs.push_back(cast<RegisterSDNode>(F->getOperand(1))->getReg());
      else {
        // Collect declared implicit uses.
        const MCInstrDesc &MCID = TII->get(F->getMachineOpcode());
        UsedRegs.append(MCID.getImplicitUses(),
                        MCID.getImplicitUses() + MCID.getNumImplicitUses());
        // In addition to declared implicit uses, we must also check for
        // direct RegisterSDNode operands.
        for (unsigned i = 0, e = F->getNumOperands(); i != e; ++i)
          if (RegisterSDNode *R = dyn_cast<RegisterSDNode>(F->getOperand(i))) {
            unsigned Reg = R->getReg();
            if (TargetRegisterInfo::isPhysicalRegister(Reg))
              UsedRegs.push_back(Reg);
          }
      }
    // Then mark unused registers as dead.
    MI->setPhysRegsDeadExcept(UsedRegs, *TRI);
  }

  // Add result register values for things that are defined by this
  // instruction.
  if (NumResults)
    CreateVirtualRegisters(Node, MI, II, IsClone, IsCloned, VRBaseMap);

  // Emit all of the actual operands of this instruction, adding them to the
  // instruction as appropriate.
  bool HasOptPRefs = II.getNumDefs() > NumResults;
  assert((!HasOptPRefs || !HasPhysRegOuts) &&
         "Unable to cope with optional defs and phys regs defs!");
  unsigned NumSkip = HasOptPRefs ? II.getNumDefs() - NumResults : 0;
  for (unsigned i = NumSkip; i != NodeOperands; ++i)
    AddOperand(MI, Node->getOperand(i), i-NumSkip+II.getNumDefs(), &II,
               VRBaseMap, /*IsDebug=*/false, IsClone, IsCloned);

  // Transfer all of the memory reference descriptions of this instruction.
  MI->setMemRefs(cast<MachineSDNode>(Node)->memoperands_begin(),
                 cast<MachineSDNode>(Node)->memoperands_end());

  // Insert the instruction into position in the block. This needs to
  // happen before any custom inserter hook is called so that the
  // hook knows where in the block to insert the replacement code.
  MBB->insert(InsertPos, MI);

  // Additional results must be physical register defs.
  if (HasPhysRegOuts) {
    for (unsigned i = II.getNumDefs(); i < NumResults; ++i) {
      unsigned Reg = II.getImplicitDefs()[i - II.getNumDefs()];
      if (Node->hasAnyUseOfValue(i))
        EmitCopyFromReg(Node, i, IsClone, IsCloned, Reg, VRBaseMap);
      // If there are no uses, mark the register as dead now, so that
      // MachineLICM/Sink can see that it's dead. Don't do this if the
      // node has a Glue value, for the benefit of targets still using
      // Glue for values in physregs.
      else if (Node->getValueType(Node->getNumValues()-1) != MVT::Glue)
        MI->addRegisterDead(Reg, TRI);
    }
  }

  // If the instruction has implicit defs and the node doesn't, mark the
  // implicit def as dead.  If the node has any glue outputs, we don't do this
  // because we don't know what implicit defs are being used by glued nodes.
  if (Node->getValueType(Node->getNumValues()-1) != MVT::Glue)
    if (const unsigned *IDList = II.getImplicitDefs()) {
      for (unsigned i = NumResults, e = II.getNumDefs()+II.getNumImplicitDefs();
           i != e; ++i)
        MI->addRegisterDead(IDList[i-II.getNumDefs()], TRI);
    }

  // Run post-isel target hook to adjust this instruction if needed.
#ifdef NDEBUG
  if (II.hasPostISelHook())
#endif
    TLI->AdjustInstrPostInstrSelection(MI, Node);
}

/// EmitSpecialNode - Generate machine code for a target-independent node and
/// needed dependencies.
void InstrEmitter::
EmitSpecialNode(SDNode *Node, bool IsClone, bool IsCloned,
                DenseMap<SDValue, unsigned> &VRBaseMap) {
  switch (Node->getOpcode()) {
  default:
#ifndef NDEBUG
    Node->dump();
#endif
    llvm_unreachable("This target-independent node should have been selected!");
    break;
  case ISD::EntryToken:
    llvm_unreachable("EntryToken should have been excluded from the schedule!");
    break;
  case ISD::MERGE_VALUES:
  case ISD::TokenFactor: // fall thru
    break;
  case ISD::CopyToReg: {
    unsigned SrcReg;
    SDValue SrcVal = Node->getOperand(2);
    if (RegisterSDNode *R = dyn_cast<RegisterSDNode>(SrcVal))
      SrcReg = R->getReg();
    else
      SrcReg = getVR(SrcVal, VRBaseMap);

    unsigned DestReg = cast<RegisterSDNode>(Node->getOperand(1))->getReg();
    if (SrcReg == DestReg) // Coalesced away the copy? Ignore.
      break;

    BuildMI(*MBB, InsertPos, Node->getDebugLoc(), TII->get(TargetOpcode::COPY),
            DestReg).addReg(SrcReg);
    break;
  }
  case ISD::CopyFromReg: {
    unsigned SrcReg = cast<RegisterSDNode>(Node->getOperand(1))->getReg();
    EmitCopyFromReg(Node, 0, IsClone, IsCloned, SrcReg, VRBaseMap);
    break;
  }
  case ISD::EH_LABEL: {
    MCSymbol *S = cast<EHLabelSDNode>(Node)->getLabel();
    BuildMI(*MBB, InsertPos, Node->getDebugLoc(),
            TII->get(TargetOpcode::EH_LABEL)).addSym(S);
    break;
  }

  case ISD::INLINEASM: {
    unsigned NumOps = Node->getNumOperands();
    if (Node->getOperand(NumOps-1).getValueType() == MVT::Glue)
      --NumOps;  // Ignore the glue operand.

    // Create the inline asm machine instruction.
    MachineInstr *MI = BuildMI(*MF, Node->getDebugLoc(),
                               TII->get(TargetOpcode::INLINEASM));

    // Add the asm string as an external symbol operand.
    SDValue AsmStrV = Node->getOperand(InlineAsm::Op_AsmString);
    const char *AsmStr = cast<ExternalSymbolSDNode>(AsmStrV)->getSymbol();
    MI->addOperand(MachineOperand::CreateES(AsmStr));

    // Add the HasSideEffect and isAlignStack bits.
    int64_t ExtraInfo =
      cast<ConstantSDNode>(Node->getOperand(InlineAsm::Op_ExtraInfo))->
                          getZExtValue();
    MI->addOperand(MachineOperand::CreateImm(ExtraInfo));

    // Add all of the operand registers to the instruction.
    for (unsigned i = InlineAsm::Op_FirstOperand; i != NumOps;) {
      unsigned Flags =
        cast<ConstantSDNode>(Node->getOperand(i))->getZExtValue();
      unsigned NumVals = InlineAsm::getNumOperandRegisters(Flags);

      MI->addOperand(MachineOperand::CreateImm(Flags));
      ++i;  // Skip the ID value.

      switch (InlineAsm::getKind(Flags)) {
      default: llvm_unreachable("Bad flags!");
        case InlineAsm::Kind_RegDef:
        for (; NumVals; --NumVals, ++i) {
          unsigned Reg = cast<RegisterSDNode>(Node->getOperand(i))->getReg();
          // FIXME: Add dead flags for physical and virtual registers defined.
          // For now, mark physical register defs as implicit to help fast
          // regalloc. This makes inline asm look a lot like calls.
          MI->addOperand(MachineOperand::CreateReg(Reg, true,
                       /*isImp=*/ TargetRegisterInfo::isPhysicalRegister(Reg)));
        }
        break;
      case InlineAsm::Kind_RegDefEarlyClobber:
      case InlineAsm::Kind_Clobber:
        for (; NumVals; --NumVals, ++i) {
          unsigned Reg = cast<RegisterSDNode>(Node->getOperand(i))->getReg();
          MI->addOperand(MachineOperand::CreateReg(Reg, /*isDef=*/ true,
                         /*isImp=*/ TargetRegisterInfo::isPhysicalRegister(Reg),
                                                   /*isKill=*/ false,
                                                   /*isDead=*/ false,
                                                   /*isUndef=*/false,
                                                   /*isEarlyClobber=*/ true));
        }
        break;
      case InlineAsm::Kind_RegUse:  // Use of register.
      case InlineAsm::Kind_Imm:  // Immediate.
      case InlineAsm::Kind_Mem:  // Addressing mode.
        // The addressing mode has been selected, just add all of the
        // operands to the machine instruction.
        for (; NumVals; --NumVals, ++i)
          AddOperand(MI, Node->getOperand(i), 0, 0, VRBaseMap,
                     /*IsDebug=*/false, IsClone, IsCloned);
        break;
      }
    }

    // Get the mdnode from the asm if it exists and add it to the instruction.
    SDValue MDV = Node->getOperand(InlineAsm::Op_MDNode);
    const MDNode *MD = cast<MDNodeSDNode>(MDV)->getMD();
    if (MD)
      MI->addOperand(MachineOperand::CreateMetadata(MD));

    MBB->insert(InsertPos, MI);
    break;
  }
  }
}

/// InstrEmitter - Construct an InstrEmitter and set it to start inserting
/// at the given position in the given block.
InstrEmitter::InstrEmitter(MachineBasicBlock *mbb,
                           MachineBasicBlock::iterator insertpos)
  : MF(mbb->getParent()),
    MRI(&MF->getRegInfo()),
    TM(&MF->getTarget()),
    TII(TM->getInstrInfo()),
    TRI(TM->getRegisterInfo()),
    TLI(TM->getTargetLowering()),
    MBB(mbb), InsertPos(insertpos) {
}
