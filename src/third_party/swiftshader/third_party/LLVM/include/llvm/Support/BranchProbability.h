//===- BranchProbability.h - Branch Probability Wrapper ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Definition of BranchProbability shared by IR and Machine Instructions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_BRANCHPROBABILITY_H
#define LLVM_SUPPORT_BRANCHPROBABILITY_H

#include "llvm/Support/DataTypes.h"

namespace llvm {

class raw_ostream;

// This class represents Branch Probability as a non-negative fraction.
class BranchProbability {

  // Numerator
  uint32_t N;

  // Denominator
  uint32_t D;

public:
  BranchProbability(uint32_t n, uint32_t d);

  uint32_t getNumerator() const { return N; }
  uint32_t getDenominator() const { return D; }

  // Return (1 - Probability).
  BranchProbability getCompl() {
    return BranchProbability(D - N, D);
  }

  void print(raw_ostream &OS) const;

  void dump() const;
};

raw_ostream &operator<<(raw_ostream &OS, const BranchProbability &Prob);

}

#endif
