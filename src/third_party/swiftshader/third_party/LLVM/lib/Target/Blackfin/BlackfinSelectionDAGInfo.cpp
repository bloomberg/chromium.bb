//===-- BlackfinSelectionDAGInfo.cpp - Blackfin SelectionDAG Info ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the BlackfinSelectionDAGInfo class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "blackfin-selectiondag-info"
#include "BlackfinTargetMachine.h"
using namespace llvm;

BlackfinSelectionDAGInfo::BlackfinSelectionDAGInfo(
                                              const BlackfinTargetMachine &TM)
  : TargetSelectionDAGInfo(TM) {
}

BlackfinSelectionDAGInfo::~BlackfinSelectionDAGInfo() {
}
