/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif


#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/actual_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/baseline_vs_baseline.h"
#include "native_client/src/trusted/validator_arm/actual_classes.h"
#include "native_client/src/trusted/validator_arm/baseline_classes.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"
#include "native_client/src/trusted/validator_arm/arm_helpers.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_named_bases.h"

using nacl_arm_dec::Instruction;
using nacl_arm_dec::ClassDecoder;
using nacl_arm_dec::Register;
using nacl_arm_dec::RegisterList;

namespace nacl_arm_test {

// The following classes are derived class decoder testers that
// add row pattern constraints and decoder restrictions to each tester.
// This is done so that it can be used to make sure that the
// corresponding pattern is not tested for cases that would be excluded
//  due to row checks, or restrictions specified by the row restrictions.


// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0,
//       pattern: cccc01110000ddddaaaammmm00m1nnnn,
//       rule: SMLAD,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTesterCase0
    : public Binary4RegisterDualOpTester {
 public:
  Binary4RegisterDualOpNoCondsUpdateTesterCase0(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpNoCondsUpdateTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~00x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000000) return false;
  // A(15:12)=1111
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpNoCondsUpdateTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Ra  ==
  //          Pc => DECODER_ERROR
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0,
//       pattern: cccc01110000dddd1111mmmm00m1nnnn,
//       rule: SMUAD,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTesterCase1
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltANoCondsUpdateTesterCase1(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltANoCondsUpdateTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~00x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000000) return false;
  // A(15:12)=~1111
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltANoCondsUpdateTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0,
//       pattern: cccc01110000ddddaaaammmm01m1nnnn,
//       rule: SMLSD,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTesterCase2
    : public Binary4RegisterDualOpTester {
 public:
  Binary4RegisterDualOpNoCondsUpdateTesterCase2(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpNoCondsUpdateTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~01x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;
  // A(15:12)=1111
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpNoCondsUpdateTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Ra  ==
  //          Pc => DECODER_ERROR
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0,
//       pattern: cccc01110000dddd1111mmmm01m1nnnn,
//       rule: SMUSD,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTesterCase3
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltANoCondsUpdateTesterCase3(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltANoCondsUpdateTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~000
  if ((inst.Bits() & 0x00700000)  !=
          0x00000000) return false;
  // op2(7:5)=~01x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;
  // A(15:12)=~1111
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltANoCondsUpdateTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0,
//       pattern: cccc01110001dddd1111mmmm0001nnnn,
//       rule: SDIV,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTesterCase4
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltANoCondsUpdateTesterCase4(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltANoCondsUpdateTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~001
  if ((inst.Bits() & 0x00700000)  !=
          0x00100000) return false;
  // op2(7:5)=~000
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltANoCondsUpdateTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0,
//       pattern: cccc01110011dddd1111mmmm0001nnnn,
//       rule: UDIV,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTesterCase5
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltANoCondsUpdateTesterCase5(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltANoCondsUpdateTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~011
  if ((inst.Bits() & 0x00700000)  !=
          0x00300000) return false;
  // op2(7:5)=~000
  if ((inst.Bits() & 0x000000E0)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltANoCondsUpdateTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=100 & op2(7:5)=00x
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualResultNoCondsUpdate,
//       constraints: ,
//       defs: {RdHi, RdLo},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0,
//       pattern: cccc01110100hhhhllllmmmm00m1nnnn,
//       rule: SMLALD,
//       safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdHi, RdLo, Rm, Rn}}
class Binary4RegisterDualResultNoCondsUpdateTesterCase6
    : public Binary4RegisterDualResultTester {
 public:
  Binary4RegisterDualResultNoCondsUpdateTesterCase6(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultNoCondsUpdateTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~100
  if ((inst.Bits() & 0x00700000)  !=
          0x00400000) return false;
  // op2(7:5)=~00x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualResultNoCondsUpdateTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualResultTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // safety: RdHi  ==
  //          RdLo => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (((inst.Bits() & 0x0000F000) >> 12))));

  // defs: {RdHi, RdLo};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// op1(22:20)=100 & op2(7:5)=01x
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualResultNoCondsUpdate,
//       constraints: ,
//       defs: {RdHi, RdLo},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0,
//       pattern: cccc01110100hhhhllllmmmm01m1nnnn,
//       rule: SMLSLD,
//       safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdHi, RdLo, Rm, Rn}}
class Binary4RegisterDualResultNoCondsUpdateTesterCase7
    : public Binary4RegisterDualResultTester {
 public:
  Binary4RegisterDualResultNoCondsUpdateTesterCase7(const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualResultNoCondsUpdateTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~100
  if ((inst.Bits() & 0x00700000)  !=
          0x00400000) return false;
  // op2(7:5)=~01x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000040) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualResultTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualResultNoCondsUpdateTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualResultTester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // safety: RdHi  ==
  //          RdLo => UNPREDICTABLE
  EXPECT_TRUE(((((inst.Bits() & 0x000F0000) >> 16)) != (((inst.Bits() & 0x0000F000) >> 12))));

  // defs: {RdHi, RdLo};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)))));

  return true;
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0,
//       pattern: cccc01110101ddddaaaammmm00r1nnnn,
//       rule: SMMLA,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTesterCase8
    : public Binary4RegisterDualOpTester {
 public:
  Binary4RegisterDualOpNoCondsUpdateTesterCase8(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpNoCondsUpdateTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~101
  if ((inst.Bits() & 0x00700000)  !=
          0x00500000) return false;
  // op2(7:5)=~00x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000000) return false;
  // A(15:12)=1111
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpNoCondsUpdateTesterCase8
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Ra  ==
  //          Pc => DECODER_ERROR
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0,
//       pattern: cccc01110101dddd1111mmmm00r1nnnn,
//       rule: SMMUL,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTesterCase9
    : public Binary3RegisterOpAltATester {
 public:
  Binary3RegisterOpAltANoCondsUpdateTesterCase9(const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary3RegisterOpAltANoCondsUpdateTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~101
  if ((inst.Bits() & 0x00700000)  !=
          0x00500000) return false;
  // op2(7:5)=~00x
  if ((inst.Bits() & 0x000000C0)  !=
          0x00000000) return false;
  // A(15:12)=~1111
  if ((inst.Bits() & 0x0000F000)  !=
          0x0000F000) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary3RegisterOpAltATester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary3RegisterOpAltANoCondsUpdateTesterCase9
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary3RegisterOpAltATester::
               ApplySanityChecks(inst, decoder));

  // safety: Pc in {Rd, Rm, Rn} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == ((inst.Bits() & 0x0000000F))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// op1(22:20)=101 & op2(7:5)=11x
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0,
//       pattern: cccc01110101ddddaaaammmm11r1nnnn,
//       rule: SMMLS,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTesterCase10
    : public Binary4RegisterDualOpTester {
 public:
  Binary4RegisterDualOpNoCondsUpdateTesterCase10(const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool Binary4RegisterDualOpNoCondsUpdateTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op1(22:20)=~101
  if ((inst.Bits() & 0x00700000)  !=
          0x00500000) return false;
  // op2(7:5)=~11x
  if ((inst.Bits() & 0x000000C0)  !=
          0x000000C0) return false;

  // if cond(31:28)=1111, don't test instruction.
  if ((inst.Bits() & 0xF0000000) == 0xF0000000) return false;

  // Check other preconditions defined for the base decoder.
  return Binary4RegisterDualOpTester::
      PassesParsePreconditions(inst, decoder);
}

bool Binary4RegisterDualOpNoCondsUpdateTesterCase10
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(Binary4RegisterDualOpTester::
               ApplySanityChecks(inst, decoder));

  // safety: Ra  ==
  //          Pc => DECODER_ERROR
  EXPECT_TRUE(((((inst.Bits() & 0x0000F000) >> 12)) != (15)));

  // safety: Pc in {Rd, Rn, Rm} => UNPREDICTABLE
  EXPECT_TRUE(!((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8))))));

  // defs: {Rd};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0,
//       pattern: cccc01110000ddddaaaammmm00m1nnnn,
//       rule: SMLAD,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTester_Case0
    : public Binary4RegisterDualOpNoCondsUpdateTesterCase0 {
 public:
  Binary4RegisterDualOpNoCondsUpdateTester_Case0()
    : Binary4RegisterDualOpNoCondsUpdateTesterCase0(
      state_.Binary4RegisterDualOpNoCondsUpdate_SMLAD_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0,
//       pattern: cccc01110000dddd1111mmmm00m1nnnn,
//       rule: SMUAD,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTester_Case1
    : public Binary3RegisterOpAltANoCondsUpdateTesterCase1 {
 public:
  Binary3RegisterOpAltANoCondsUpdateTester_Case1()
    : Binary3RegisterOpAltANoCondsUpdateTesterCase1(
      state_.Binary3RegisterOpAltANoCondsUpdate_SMUAD_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0,
//       pattern: cccc01110000ddddaaaammmm01m1nnnn,
//       rule: SMLSD,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTester_Case2
    : public Binary4RegisterDualOpNoCondsUpdateTesterCase2 {
 public:
  Binary4RegisterDualOpNoCondsUpdateTester_Case2()
    : Binary4RegisterDualOpNoCondsUpdateTesterCase2(
      state_.Binary4RegisterDualOpNoCondsUpdate_SMLSD_instance_)
  {}
};

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0,
//       pattern: cccc01110000dddd1111mmmm01m1nnnn,
//       rule: SMUSD,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTester_Case3
    : public Binary3RegisterOpAltANoCondsUpdateTesterCase3 {
 public:
  Binary3RegisterOpAltANoCondsUpdateTester_Case3()
    : Binary3RegisterOpAltANoCondsUpdateTesterCase3(
      state_.Binary3RegisterOpAltANoCondsUpdate_SMUSD_instance_)
  {}
};

// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0,
//       pattern: cccc01110001dddd1111mmmm0001nnnn,
//       rule: SDIV,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTester_Case4
    : public Binary3RegisterOpAltANoCondsUpdateTesterCase4 {
 public:
  Binary3RegisterOpAltANoCondsUpdateTester_Case4()
    : Binary3RegisterOpAltANoCondsUpdateTesterCase4(
      state_.Binary3RegisterOpAltANoCondsUpdate_SDIV_instance_)
  {}
};

// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0,
//       pattern: cccc01110011dddd1111mmmm0001nnnn,
//       rule: UDIV,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTester_Case5
    : public Binary3RegisterOpAltANoCondsUpdateTesterCase5 {
 public:
  Binary3RegisterOpAltANoCondsUpdateTester_Case5()
    : Binary3RegisterOpAltANoCondsUpdateTesterCase5(
      state_.Binary3RegisterOpAltANoCondsUpdate_UDIV_instance_)
  {}
};

// op1(22:20)=100 & op2(7:5)=00x
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualResultNoCondsUpdate,
//       constraints: ,
//       defs: {RdHi, RdLo},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0,
//       pattern: cccc01110100hhhhllllmmmm00m1nnnn,
//       rule: SMLALD,
//       safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdHi, RdLo, Rm, Rn}}
class Binary4RegisterDualResultNoCondsUpdateTester_Case6
    : public Binary4RegisterDualResultNoCondsUpdateTesterCase6 {
 public:
  Binary4RegisterDualResultNoCondsUpdateTester_Case6()
    : Binary4RegisterDualResultNoCondsUpdateTesterCase6(
      state_.Binary4RegisterDualResultNoCondsUpdate_SMLALD_instance_)
  {}
};

// op1(22:20)=100 & op2(7:5)=01x
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualResultNoCondsUpdate,
//       constraints: ,
//       defs: {RdHi, RdLo},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0,
//       pattern: cccc01110100hhhhllllmmmm01m1nnnn,
//       rule: SMLSLD,
//       safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdHi, RdLo, Rm, Rn}}
class Binary4RegisterDualResultNoCondsUpdateTester_Case7
    : public Binary4RegisterDualResultNoCondsUpdateTesterCase7 {
 public:
  Binary4RegisterDualResultNoCondsUpdateTester_Case7()
    : Binary4RegisterDualResultNoCondsUpdateTesterCase7(
      state_.Binary4RegisterDualResultNoCondsUpdate_SMLSLD_instance_)
  {}
};

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0,
//       pattern: cccc01110101ddddaaaammmm00r1nnnn,
//       rule: SMMLA,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTester_Case8
    : public Binary4RegisterDualOpNoCondsUpdateTesterCase8 {
 public:
  Binary4RegisterDualOpNoCondsUpdateTester_Case8()
    : Binary4RegisterDualOpNoCondsUpdateTesterCase8(
      state_.Binary4RegisterDualOpNoCondsUpdate_SMMLA_instance_)
  {}
};

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0,
//       pattern: cccc01110101dddd1111mmmm00r1nnnn,
//       rule: SMMUL,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
class Binary3RegisterOpAltANoCondsUpdateTester_Case9
    : public Binary3RegisterOpAltANoCondsUpdateTesterCase9 {
 public:
  Binary3RegisterOpAltANoCondsUpdateTester_Case9()
    : Binary3RegisterOpAltANoCondsUpdateTesterCase9(
      state_.Binary3RegisterOpAltANoCondsUpdate_SMMUL_instance_)
  {}
};

// op1(22:20)=101 & op2(7:5)=11x
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0,
//       pattern: cccc01110101ddddaaaammmm11r1nnnn,
//       rule: SMMLS,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
class Binary4RegisterDualOpNoCondsUpdateTester_Case10
    : public Binary4RegisterDualOpNoCondsUpdateTesterCase10 {
 public:
  Binary4RegisterDualOpNoCondsUpdateTester_Case10()
    : Binary4RegisterDualOpNoCondsUpdateTesterCase10(
      state_.Binary4RegisterDualOpNoCondsUpdate_SMMLS_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_0,
//       pattern: cccc01110000ddddaaaammmm00m1nnnn,
//       rule: SMLAD,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpNoCondsUpdateTester_Case0_TestCase0) {
  Binary4RegisterDualOpNoCondsUpdateTester_Case0 baseline_tester;
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLAD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm00m1nnnn");
}

// op1(22:20)=000 & op2(7:5)=00x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0,
//       pattern: cccc01110000dddd1111mmmm00m1nnnn,
//       rule: SMUAD,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltANoCondsUpdateTester_Case1_TestCase1) {
  Binary3RegisterOpAltANoCondsUpdateTester_Case1 baseline_tester;
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUAD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm00m1nnnn");
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLSD_cccc01110000ddddaaaammmm01m1nnnn_case_0,
//       pattern: cccc01110000ddddaaaammmm01m1nnnn,
//       rule: SMLSD,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpNoCondsUpdateTester_Case2_TestCase2) {
  Binary4RegisterDualOpNoCondsUpdateTester_Case2 baseline_tester;
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMLSD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000ddddaaaammmm01m1nnnn");
}

// op1(22:20)=000 & op2(7:5)=01x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0,
//       pattern: cccc01110000dddd1111mmmm01m1nnnn,
//       rule: SMUSD,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltANoCondsUpdateTester_Case3_TestCase3) {
  Binary3RegisterOpAltANoCondsUpdateTester_Case3 baseline_tester;
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMUSD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110000dddd1111mmmm01m1nnnn");
}

// op1(22:20)=001 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SDIV_cccc01110001dddd1111mmmm0001nnnn_case_0,
//       pattern: cccc01110001dddd1111mmmm0001nnnn,
//       rule: SDIV,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltANoCondsUpdateTester_Case4_TestCase4) {
  Binary3RegisterOpAltANoCondsUpdateTester_Case4 baseline_tester;
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SDIV actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110001dddd1111mmmm0001nnnn");
}

// op1(22:20)=011 & op2(7:5)=000 & $pattern(31:0)=xxxxxxxxxxxxxxxx1111xxxxxxxxxxxx
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0,
//       pattern: cccc01110011dddd1111mmmm0001nnnn,
//       rule: UDIV,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltANoCondsUpdateTester_Case5_TestCase5) {
  Binary3RegisterOpAltANoCondsUpdateTester_Case5 baseline_tester;
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_UDIV actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110011dddd1111mmmm0001nnnn");
}

// op1(22:20)=100 & op2(7:5)=00x
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualResultNoCondsUpdate,
//       constraints: ,
//       defs: {RdHi, RdLo},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_0,
//       pattern: cccc01110100hhhhllllmmmm00m1nnnn,
//       rule: SMLALD,
//       safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdHi, RdLo, Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultNoCondsUpdateTester_Case6_TestCase6) {
  Binary4RegisterDualResultNoCondsUpdateTester_Case6 baseline_tester;
  NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLALD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm00m1nnnn");
}

// op1(22:20)=100 & op2(7:5)=01x
//    = {Pc: 15,
//       RdHi: RdHi(19:16),
//       RdLo: RdLo(15:12),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualResultNoCondsUpdate,
//       constraints: ,
//       defs: {RdHi, RdLo},
//       fields: [RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMLSLD_cccc01110100hhhhllllmmmm01m1nnnn_case_0,
//       pattern: cccc01110100hhhhllllmmmm01m1nnnn,
//       rule: SMLSLD,
//       safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//         RdHi  ==
//               RdLo => UNPREDICTABLE],
//       uses: {RdHi, RdLo, Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualResultNoCondsUpdateTester_Case7_TestCase7) {
  Binary4RegisterDualResultNoCondsUpdateTester_Case7 baseline_tester;
  NamedActual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1_SMLSLD actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110100hhhhllllmmmm01m1nnnn");
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=~1111
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0,
//       pattern: cccc01110101ddddaaaammmm00r1nnnn,
//       rule: SMMLA,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpNoCondsUpdateTester_Case8_TestCase8) {
  Binary4RegisterDualOpNoCondsUpdateTester_Case8 baseline_tester;
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLA actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm00r1nnnn");
}

// op1(22:20)=101 & op2(7:5)=00x & A(15:12)=1111
//    = {Pc: 15,
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1,
//       baseline: Binary3RegisterOpAltANoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0,
//       pattern: cccc01110101dddd1111mmmm00r1nnnn,
//       rule: SMMUL,
//       safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//       uses: {Rm, Rn}}
TEST_F(Arm32DecoderStateTests,
       Binary3RegisterOpAltANoCondsUpdateTester_Case9_TestCase9) {
  Binary3RegisterOpAltANoCondsUpdateTester_Case9 baseline_tester;
  NamedActual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1_SMMUL actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101dddd1111mmmm00r1nnnn");
}

// op1(22:20)=101 & op2(7:5)=11x
//    = {Pc: 15,
//       Ra: Ra(15:12),
//       Rd: Rd(19:16),
//       Rm: Rm(11:8),
//       Rn: Rn(3:0),
//       actual: Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1,
//       baseline: Binary4RegisterDualOpNoCondsUpdate,
//       constraints: ,
//       defs: {Rd},
//       fields: [Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//       generated_baseline: SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0,
//       pattern: cccc01110101ddddaaaammmm11r1nnnn,
//       rule: SMMLS,
//       safety: [Ra  ==
//               Pc => DECODER_ERROR,
//         Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//       uses: {Rn, Rm, Ra}}
TEST_F(Arm32DecoderStateTests,
       Binary4RegisterDualOpNoCondsUpdateTester_Case10_TestCase10) {
  Binary4RegisterDualOpNoCondsUpdateTester_Case10 baseline_tester;
  NamedActual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1_SMMLS actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("cccc01110101ddddaaaammmm11r1nnnn");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
