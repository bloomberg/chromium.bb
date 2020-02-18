//===- subzero/src/IceConditionCodesX8664.h - Condition Codes ---*- C++ -*-===//
//
//                        The Subzero Code Generator
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Declares the condition codes for x86-64.
///
//===----------------------------------------------------------------------===//

#ifndef SUBZERO_SRC_ICECONDITIONCODESX8664_H
#define SUBZERO_SRC_ICECONDITIONCODESX8664_H

#include "IceDefs.h"
#include "IceInstX8664.def"

namespace Ice {

class CondX8664 {
public:
  /// An enum of condition codes used for branches and cmov. The enum value
  /// should match the value used to encode operands in binary instructions.
  enum BrCond {
#define X(val, encode, opp, dump, emit) val = encode,
    ICEINSTX8664BR_TABLE
#undef X
        Br_None
  };

  /// An enum of condition codes relevant to the CMPPS instruction. The enum
  /// value should match the value used to encode operands in binary
  /// instructions.
  enum CmppsCond {
#define X(val, emit) val,
    ICEINSTX8664CMPPS_TABLE
#undef X
        Cmpps_Invalid
  };
};

} // end of namespace Ice

#endif // SUBZERO_SRC_ICECONDITIONCODESX8664_H
