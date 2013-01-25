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


// op(25:20)=0000x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase0
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase0(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0000x0
  if ((inst.Bits() & 0x03D00000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase0
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) &&
  //       Rn  !=
  //          SmallestGPR(registers) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16)))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) != (nacl_arm_dec::SmallestGPR(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF))))))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// op(25:20)=0000x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase1
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase1(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0000x1
  if ((inst.Bits() & 0x03D00000)  !=
          0x00100000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase1
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: Contains(registers, Pc) => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(15))));

  // defs: Union({Rn
  //       if wback
  //       else None}, registers);
  EXPECT_TRUE(decoder.defs(inst).IsSame(nacl_arm_dec::Union(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32))), nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))));

  return true;
}

// op(25:20)=0010x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase2
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase2(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0010x0
  if ((inst.Bits() & 0x03D00000)  !=
          0x00800000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase2
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) &&
  //       Rn  !=
  //          SmallestGPR(registers) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16)))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) != (nacl_arm_dec::SmallestGPR(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF))))))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// op(25:20)=0010x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase3
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase3(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0010x1
  if ((inst.Bits() & 0x03D00000)  !=
          0x00900000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase3
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: Contains(registers, Pc) => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(15))));

  // defs: Union({Rn
  //       if wback
  //       else None}, registers);
  EXPECT_TRUE(decoder.defs(inst).IsSame(nacl_arm_dec::Union(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32))), nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))));

  return true;
}

// op(25:20)=0100x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase4
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase4(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0100x0
  if ((inst.Bits() & 0x03D00000)  !=
          0x01000000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase4
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) &&
  //       Rn  !=
  //          SmallestGPR(registers) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16)))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) != (nacl_arm_dec::SmallestGPR(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF))))))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// op(25:20)=0100x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase5
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase5(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0100x1
  if ((inst.Bits() & 0x03D00000)  !=
          0x01100000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase5
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: Contains(registers, Pc) => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(15))));

  // defs: Union({Rn
  //       if wback
  //       else None}, registers);
  EXPECT_TRUE(decoder.defs(inst).IsSame(nacl_arm_dec::Union(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32))), nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))));

  return true;
}

// op(25:20)=0110x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase6
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase6(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0110x0
  if ((inst.Bits() & 0x03D00000)  !=
          0x01800000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase6
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) &&
  //       Rn  !=
  //          SmallestGPR(registers) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16)))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) != (nacl_arm_dec::SmallestGPR(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF))))))));

  // defs: {Rn
  //       if wback
  //       else None};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)))));

  return true;
}

// op(25:20)=0110x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadStoreRegisterListTesterCase7
    : public LoadStoreRegisterListTester {
 public:
  LoadStoreRegisterListTesterCase7(const NamedClassDecoder& decoder)
    : LoadStoreRegisterListTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool LoadStoreRegisterListTesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0110x1
  if ((inst.Bits() & 0x03D00000)  !=
          0x01900000) return false;

  // Check other preconditions defined for the base decoder.
  return LoadStoreRegisterListTester::
      PassesParsePreconditions(inst, decoder);
}

bool LoadStoreRegisterListTesterCase7
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStoreRegisterListTester::
               ApplySanityChecks(inst, decoder));

  // safety: Rn  ==
  //          Pc ||
  //       NumGPRs(registers)  <
  //          1 => UNPREDICTABLE
  EXPECT_TRUE(!((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1)))));

  // safety: wback &&
  //       Contains(registers, Rn) => UNKNOWN
  EXPECT_TRUE(!(((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16))))));

  // safety: Contains(registers, Pc) => FORBIDDEN_OPERANDS
  EXPECT_TRUE(!(nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(15))));

  // defs: Union({Rn
  //       if wback
  //       else None}, registers);
  EXPECT_TRUE(decoder.defs(inst).IsSame(nacl_arm_dec::Union(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32))), nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))));

  return true;
}

// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase8
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase8(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0xx1x0
  if ((inst.Bits() & 0x02500000)  !=
          0x00400000) return false;
  // $pattern(31:0)=~xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0xx1x1 & R(15)=0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase9
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase9(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0xx1x1
  if ((inst.Bits() & 0x02500000)  !=
          0x00500000) return false;
  // R(15)=~0
  if ((inst.Bits() & 0x00008000)  !=
          0x00000000) return false;
  // $pattern(31:0)=~xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=0xx1x1 & R(15)=1
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: }
class UnsafeCondDecoderTesterCase10
    : public UnsafeCondDecoderTester {
 public:
  UnsafeCondDecoderTesterCase10(const NamedClassDecoder& decoder)
    : UnsafeCondDecoderTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool UnsafeCondDecoderTesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~0xx1x1
  if ((inst.Bits() & 0x02500000)  !=
          0x00500000) return false;
  // R(15)=~1
  if ((inst.Bits() & 0x00008000)  !=
          0x00008000) return false;

  // Check other preconditions defined for the base decoder.
  return UnsafeCondDecoderTester::
      PassesParsePreconditions(inst, decoder);
}

// op(25:20)=10xxxx
//    = {Pc: 15,
//       actual: BranchImmediate24,
//       baseline: BranchImmediate24,
//       constraints: ,
//       defs: {Pc},
//       fields: [imm24(23:0)],
//       generated_baseline: B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       imm24: imm24(23:0),
//       imm32: SignExtend(imm24:0(1:0), 32),
//       relative: true,
//       relative_offset: imm32,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
class BranchImmediate24TesterCase11
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24TesterCase11(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BranchImmediate24TesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~10xxxx
  if ((inst.Bits() & 0x03000000)  !=
          0x02000000) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

bool BranchImmediate24TesterCase11
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BranchImmediate24Tester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {Pc};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(15))));

  return true;
}

// op(25:20)=11xxxx
//    = {Lr: 14,
//       Pc: 15,
//       actual: BranchImmediate24,
//       baseline: BranchImmediate24,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [imm24(23:0)],
//       generated_baseline: BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       imm24: imm24(23:0),
//       imm32: SignExtend(imm24:0(1:0), 32),
//       relative: true,
//       relative_offset: imm32,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
class BranchImmediate24TesterCase12
    : public BranchImmediate24Tester {
 public:
  BranchImmediate24TesterCase12(const NamedClassDecoder& decoder)
    : BranchImmediate24Tester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
  virtual bool ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const NamedClassDecoder& decoder);
};

bool BranchImmediate24TesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // op(25:20)=~11xxxx
  if ((inst.Bits() & 0x03000000)  !=
          0x03000000) return false;

  // Check other preconditions defined for the base decoder.
  return BranchImmediate24Tester::
      PassesParsePreconditions(inst, decoder);
}

bool BranchImmediate24TesterCase12
::ApplySanityChecks(nacl_arm_dec::Instruction inst,
                    const NamedClassDecoder& decoder) {
  NC_PRECOND(BranchImmediate24Tester::
               ApplySanityChecks(inst, decoder));

  // safety: true => MAY_BE_SAFE
  EXPECT_TRUE(true);

  // defs: {Pc, Lr};
  EXPECT_TRUE(decoder.defs(inst).IsSame(RegisterList().
   Add(Register(15)).
   Add(Register(14))));

  return true;
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// op(25:20)=0000x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STMDA_STMED,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class StoreRegisterListTester_Case0
    : public LoadStoreRegisterListTesterCase0 {
 public:
  StoreRegisterListTester_Case0()
    : LoadStoreRegisterListTesterCase0(
      state_.StoreRegisterList_STMDA_STMED_instance_)
  {}
};

// op(25:20)=0000x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDMDA_LDMFA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadRegisterListTester_Case1
    : public LoadStoreRegisterListTesterCase1 {
 public:
  LoadRegisterListTester_Case1()
    : LoadStoreRegisterListTesterCase1(
      state_.LoadRegisterList_LDMDA_LDMFA_instance_)
  {}
};

// op(25:20)=0010x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STM_STMIA_STMEA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class StoreRegisterListTester_Case2
    : public LoadStoreRegisterListTesterCase2 {
 public:
  StoreRegisterListTester_Case2()
    : LoadStoreRegisterListTesterCase2(
      state_.StoreRegisterList_STM_STMIA_STMEA_instance_)
  {}
};

// op(25:20)=0010x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDM_LDMIA_LDMFD,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadRegisterListTester_Case3
    : public LoadStoreRegisterListTesterCase3 {
 public:
  LoadRegisterListTester_Case3()
    : LoadStoreRegisterListTesterCase3(
      state_.LoadRegisterList_LDM_LDMIA_LDMFD_instance_)
  {}
};

// op(25:20)=0100x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STMDB_STMFD,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class StoreRegisterListTester_Case4
    : public LoadStoreRegisterListTesterCase4 {
 public:
  StoreRegisterListTester_Case4()
    : LoadStoreRegisterListTesterCase4(
      state_.StoreRegisterList_STMDB_STMFD_instance_)
  {}
};

// op(25:20)=0100x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDMDB_LDMEA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadRegisterListTester_Case5
    : public LoadStoreRegisterListTesterCase5 {
 public:
  LoadRegisterListTester_Case5()
    : LoadStoreRegisterListTesterCase5(
      state_.LoadRegisterList_LDMDB_LDMEA_instance_)
  {}
};

// op(25:20)=0110x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STMIB_STMFA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
class StoreRegisterListTester_Case6
    : public LoadStoreRegisterListTesterCase6 {
 public:
  StoreRegisterListTester_Case6()
    : LoadStoreRegisterListTesterCase6(
      state_.StoreRegisterList_STMIB_STMFA_instance_)
  {}
};

// op(25:20)=0110x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDMIB_LDMED,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
class LoadRegisterListTester_Case7
    : public LoadStoreRegisterListTesterCase7 {
 public:
  LoadRegisterListTester_Case7()
    : LoadStoreRegisterListTesterCase7(
      state_.LoadRegisterList_LDMIB_LDMED_instance_)
  {}
};

// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: STM_User_registers}
class ForbiddenCondDecoderTester_Case8
    : public UnsafeCondDecoderTesterCase8 {
 public:
  ForbiddenCondDecoderTester_Case8()
    : UnsafeCondDecoderTesterCase8(
      state_.ForbiddenCondDecoder_STM_User_registers_instance_)
  {}
};

// op(25:20)=0xx1x1 & R(15)=0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: LDM_User_registers}
class ForbiddenCondDecoderTester_Case9
    : public UnsafeCondDecoderTesterCase9 {
 public:
  ForbiddenCondDecoderTester_Case9()
    : UnsafeCondDecoderTesterCase9(
      state_.ForbiddenCondDecoder_LDM_User_registers_instance_)
  {}
};

// op(25:20)=0xx1x1 & R(15)=1
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       rule: LDM_exception_return}
class ForbiddenCondDecoderTester_Case10
    : public UnsafeCondDecoderTesterCase10 {
 public:
  ForbiddenCondDecoderTester_Case10()
    : UnsafeCondDecoderTesterCase10(
      state_.ForbiddenCondDecoder_LDM_exception_return_instance_)
  {}
};

// op(25:20)=10xxxx
//    = {Pc: 15,
//       actual: BranchImmediate24,
//       baseline: BranchImmediate24,
//       constraints: ,
//       defs: {Pc},
//       fields: [imm24(23:0)],
//       generated_baseline: B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       imm24: imm24(23:0),
//       imm32: SignExtend(imm24:0(1:0), 32),
//       relative: true,
//       relative_offset: imm32,
//       rule: B,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
class BranchImmediate24Tester_Case11
    : public BranchImmediate24TesterCase11 {
 public:
  BranchImmediate24Tester_Case11()
    : BranchImmediate24TesterCase11(
      state_.BranchImmediate24_B_instance_)
  {}
};

// op(25:20)=11xxxx
//    = {Lr: 14,
//       Pc: 15,
//       actual: BranchImmediate24,
//       baseline: BranchImmediate24,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [imm24(23:0)],
//       generated_baseline: BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       imm24: imm24(23:0),
//       imm32: SignExtend(imm24:0(1:0), 32),
//       relative: true,
//       relative_offset: imm32,
//       rule: BL_BLX_immediate,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
class BranchImmediate24Tester_Case12
    : public BranchImmediate24TesterCase12 {
 public:
  BranchImmediate24Tester_Case12()
    : BranchImmediate24TesterCase12(
      state_.BranchImmediate24_BL_BLX_immediate_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// op(25:20)=0000x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100000w0nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STMDA_STMED,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case0_TestCase0) {
  StoreRegisterListTester_Case0 tester;
  tester.Test("cccc100000w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0000x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100000w1nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDMDA_LDMFA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case1_TestCase1) {
  LoadRegisterListTester_Case1 tester;
  tester.Test("cccc100000w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0010x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100010w0nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STM_STMIA_STMEA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case2_TestCase2) {
  StoreRegisterListTester_Case2 tester;
  tester.Test("cccc100010w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0010x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDM_LDMIA_LDMFD_cccc100010w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100010w1nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDM_LDMIA_LDMFD,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case3_TestCase3) {
  LoadRegisterListTester_Case3 tester;
  tester.Test("cccc100010w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0100x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100100w0nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STMDB_STMFD,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case4_TestCase4) {
  StoreRegisterListTester_Case4 tester;
  tester.Test("cccc100100w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0100x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMDB_LDMEA_cccc100100w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100100w1nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDMDB_LDMEA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case5_TestCase5) {
  LoadRegisterListTester_Case5 tester;
  tester.Test("cccc100100w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0110x0
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: StoreRegisterList,
//       base: Rn,
//       baseline: StoreRegisterList,
//       constraints: ,
//       defs: {Rn
//            if wback
//            else None},
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100110w0nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: STMIB_STMFA,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) &&
//            Rn  !=
//               SmallestGPR(registers) => UNKNOWN],
//       small_imm_base_wb: true,
//       true: true,
//       uses: Union({Rn}, registers),
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       StoreRegisterListTester_Case6_TestCase6) {
  StoreRegisterListTester_Case6 tester;
  tester.Test("cccc100110w0nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0110x1
//    = {None: 32,
//       Pc: 15,
//       Rn: Rn(19:16),
//       W: W(21),
//       actual: LoadRegisterList,
//       base: Rn,
//       baseline: LoadRegisterList,
//       constraints: ,
//       defs: Union({Rn
//            if wback
//            else None}, registers),
//       fields: [W(21), Rn(19:16), register_list(15:0)],
//       generated_baseline: LDMIB_LDMED_cccc100110w1nnnnrrrrrrrrrrrrrrrr_case_0,
//       pattern: cccc100110w1nnnnrrrrrrrrrrrrrrrr,
//       register_list: register_list(15:0),
//       registers: RegisterList(register_list),
//       rule: LDMIB_LDMED,
//       safety: [Rn  ==
//               Pc ||
//            NumGPRs(registers)  <
//               1 => UNPREDICTABLE,
//         wback &&
//            Contains(registers, Rn) => UNKNOWN,
//         Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//       small_imm_base_wb: true,
//       true: true,
//       uses: {Rn},
//       wback: W(21)=1}
TEST_F(Arm32DecoderStateTests,
       LoadRegisterListTester_Case7_TestCase7) {
  LoadRegisterListTester_Case7 tester;
  tester.Test("cccc100110w1nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0xx1x0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc100pu100nnnnrrrrrrrrrrrrrrrr,
//       rule: STM_User_registers}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case8_TestCase8) {
  ForbiddenCondDecoderTester_Case8 tester;
  tester.Test("cccc100pu100nnnnrrrrrrrrrrrrrrrr");
}

// op(25:20)=0xx1x1 & R(15)=0 & $pattern(31:0)=xxxxxxxxxx0xxxxxxxxxxxxxxxxxxxxx
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc100pu101nnnn0rrrrrrrrrrrrrrr,
//       rule: LDM_User_registers}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case9_TestCase9) {
  ForbiddenCondDecoderTester_Case9 tester;
  tester.Test("cccc100pu101nnnn0rrrrrrrrrrrrrrr");
}

// op(25:20)=0xx1x1 & R(15)=1
//    = {actual: ForbiddenCondDecoder,
//       baseline: ForbiddenCondDecoder,
//       constraints: ,
//       pattern: cccc100pu1w1nnnn1rrrrrrrrrrrrrrr,
//       rule: LDM_exception_return}
TEST_F(Arm32DecoderStateTests,
       ForbiddenCondDecoderTester_Case10_TestCase10) {
  ForbiddenCondDecoderTester_Case10 tester;
  tester.Test("cccc100pu1w1nnnn1rrrrrrrrrrrrrrr");
}

// op(25:20)=10xxxx
//    = {Pc: 15,
//       actual: BranchImmediate24,
//       baseline: BranchImmediate24,
//       constraints: ,
//       defs: {Pc},
//       fields: [imm24(23:0)],
//       generated_baseline: B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       imm24: imm24(23:0),
//       imm32: SignExtend(imm24:0(1:0), 32),
//       pattern: cccc1010iiiiiiiiiiiiiiiiiiiiiiii,
//       relative: true,
//       relative_offset: imm32,
//       rule: B,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_Case11_TestCase11) {
  BranchImmediate24Tester_Case11 tester;
  tester.Test("cccc1010iiiiiiiiiiiiiiiiiiiiiiii");
}

// op(25:20)=11xxxx
//    = {Lr: 14,
//       Pc: 15,
//       actual: BranchImmediate24,
//       baseline: BranchImmediate24,
//       constraints: ,
//       defs: {Pc, Lr},
//       fields: [imm24(23:0)],
//       generated_baseline: BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//       imm24: imm24(23:0),
//       imm32: SignExtend(imm24:0(1:0), 32),
//       pattern: cccc1011iiiiiiiiiiiiiiiiiiiiiiii,
//       relative: true,
//       relative_offset: imm32,
//       rule: BL_BLX_immediate,
//       safety: [true => MAY_BE_SAFE],
//       true: true,
//       uses: {Pc}}
TEST_F(Arm32DecoderStateTests,
       BranchImmediate24Tester_Case12_TestCase12) {
  BranchImmediate24Tester_Case12 tester;
  tester.Test("cccc1011iiiiiiiiiiiiiiiiiiiiiiii");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
