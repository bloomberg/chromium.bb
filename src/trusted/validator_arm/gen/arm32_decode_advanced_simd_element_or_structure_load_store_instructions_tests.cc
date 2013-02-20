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


// L(21)=0 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1TesterCase0
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple1TesterCase0(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple1TesterCase0
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2TesterCase1
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple2TesterCase1(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple2TesterCase1
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1TesterCase2
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple1TesterCase2(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple1TesterCase2
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       generated_baseline: VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple4TesterCase3
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple4TesterCase3(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple4TesterCase3
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~000x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple3TesterCase4
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple3TesterCase4(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple3TesterCase4
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~010x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1TesterCase5
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple1TesterCase5(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple1TesterCase5
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~011x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2TesterCase6
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple2TesterCase6(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple2TesterCase6
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~100x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=1000
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1TesterCase7
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle1TesterCase7(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle1TesterCase7
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2TesterCase8
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle2TesterCase8(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle2TesterCase8
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3TesterCase9
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle3TesterCase9(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle3TesterCase9
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4TesterCase10
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle4TesterCase10(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle4TesterCase10
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=0x00
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1TesterCase11
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle1TesterCase11(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle1TesterCase11
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x00
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2TesterCase12
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle2TesterCase12(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle2TesterCase12
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x01
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3TesterCase13
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle3TesterCase13(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle3TesterCase13
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x10
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=0 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4TesterCase14
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle4TesterCase14(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle4TesterCase14
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~0
  if ((inst.Bits() & 0x00200000)  !=
          0x00000000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x11
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1TesterCase15
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple1TesterCase15(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple1TesterCase15
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2TesterCase16
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple2TesterCase16(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple2TesterCase16
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~0011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1TesterCase17
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple1TesterCase17(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple1TesterCase17
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       generated_baseline: VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple4TesterCase18
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple4TesterCase18(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple4TesterCase18
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~000x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple3TesterCase19
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple3TesterCase19(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple3TesterCase19
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~010x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000400) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1TesterCase20
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple1TesterCase20(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple1TesterCase20
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~011x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000600) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2TesterCase21
    : public VectorLoadStoreMultipleTester {
 public:
  VectorLoadStoreMultiple2TesterCase21(const NamedClassDecoder& decoder)
    : VectorLoadStoreMultipleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreMultiple2TesterCase21
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~0
  if ((inst.Bits() & 0x00800000)  !=
          0x00000000) return false;
  // B(11:8)=~100x
  if ((inst.Bits() & 0x00000E00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreMultipleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1000
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1TesterCase22
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle1TesterCase22(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle1TesterCase22
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1000
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000800) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2TesterCase23
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle2TesterCase23(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle2TesterCase23
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1001
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000900) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3TesterCase24
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle3TesterCase24(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle3TesterCase24
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1010
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000A00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4TesterCase25
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle4TesterCase25(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle4TesterCase25
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1011
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000B00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1100
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle1AllLanes,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1100sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if T(5)=0
//            else 2,
//       rule: VLD1_single_element_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            (size(7:6)=00 &&
//            a(4)=1) => UNDEFINED,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle1AllLanesTesterCase26
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingle1AllLanesTesterCase26(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadSingle1AllLanesTesterCase26
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1100
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000C00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1101
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle2AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22), Rn(19:16), Vd(15:12), size(7:6), T(5), Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1101sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle2AllLanesTesterCase27
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingle2AllLanesTesterCase27(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadSingle2AllLanesTesterCase27
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1101
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000D00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1110
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle3AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1110sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            a(4)=1 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle3AllLanesTesterCase28
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingle3AllLanesTesterCase28(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadSingle3AllLanesTesterCase28
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1110
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000E00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=1111
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle4AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1111sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 &&
//            a(4)=0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle4AllLanesTesterCase29
    : public VectorLoadSingleAllLanesTester {
 public:
  VectorLoadSingle4AllLanesTesterCase29(const NamedClassDecoder& decoder)
    : VectorLoadSingleAllLanesTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadSingle4AllLanesTesterCase29
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~1111
  if ((inst.Bits() & 0x00000F00)  !=
          0x00000F00) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadSingleAllLanesTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=0x00
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1TesterCase30
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle1TesterCase30(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle1TesterCase30
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x00
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000000) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2TesterCase31
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle2TesterCase31(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle2TesterCase31
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x01
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000100) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3TesterCase32
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle3TesterCase32(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle3TesterCase32
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x10
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000200) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// L(21)=1 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4TesterCase33
    : public VectorLoadStoreSingleTester {
 public:
  VectorLoadStoreSingle4TesterCase33(const NamedClassDecoder& decoder)
    : VectorLoadStoreSingleTester(decoder) {}
  virtual bool PassesParsePreconditions(
      nacl_arm_dec::Instruction inst,
      const NamedClassDecoder& decoder);
};

bool VectorLoadStoreSingle4TesterCase33
::PassesParsePreconditions(
     nacl_arm_dec::Instruction inst,
     const NamedClassDecoder& decoder) {

  // Check that row patterns apply to pattern being checked.'
  // L(21)=~1
  if ((inst.Bits() & 0x00200000)  !=
          0x00200000) return false;
  // A(23)=~1
  if ((inst.Bits() & 0x00800000)  !=
          0x00800000) return false;
  // B(11:8)=~0x11
  if ((inst.Bits() & 0x00000B00)  !=
          0x00000300) return false;

  // Check other preconditions defined for the base decoder.
  return VectorLoadStoreSingleTester::
      PassesParsePreconditions(inst, decoder);
}

// The following are derived class decoder testers for decoder actions
// associated with a pattern of an action. These derived classes introduce
// a default constructor that automatically initializes the expected decoder
// to the corresponding instance in the generated DecoderState.

// L(21)=0 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case0
    : public VectorLoadStoreMultiple1TesterCase0 {
 public:
  VectorLoadStoreMultiple1Tester_Case0()
    : VectorLoadStoreMultiple1TesterCase0(
      state_.VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// L(21)=0 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case1
    : public VectorLoadStoreMultiple2TesterCase1 {
 public:
  VectorLoadStoreMultiple2Tester_Case1()
    : VectorLoadStoreMultiple2TesterCase1(
      state_.VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_)
  {}
};

// L(21)=0 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case2
    : public VectorLoadStoreMultiple1TesterCase2 {
 public:
  VectorLoadStoreMultiple1Tester_Case2()
    : VectorLoadStoreMultiple1TesterCase2(
      state_.VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// L(21)=0 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       generated_baseline: VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple4Tester_Case3
    : public VectorLoadStoreMultiple4TesterCase3 {
 public:
  VectorLoadStoreMultiple4Tester_Case3()
    : VectorLoadStoreMultiple4TesterCase3(
      state_.VectorLoadStoreMultiple4_VST4_multiple_4_element_structures_instance_)
  {}
};

// L(21)=0 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple3Tester_Case4
    : public VectorLoadStoreMultiple3TesterCase4 {
 public:
  VectorLoadStoreMultiple3Tester_Case4()
    : VectorLoadStoreMultiple3TesterCase4(
      state_.VectorLoadStoreMultiple3_VST3_multiple_3_element_structures_instance_)
  {}
};

// L(21)=0 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case5
    : public VectorLoadStoreMultiple1TesterCase5 {
 public:
  VectorLoadStoreMultiple1Tester_Case5()
    : VectorLoadStoreMultiple1TesterCase5(
      state_.VectorLoadStoreMultiple1_VST1_multiple_single_elements_instance_)
  {}
};

// L(21)=0 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case6
    : public VectorLoadStoreMultiple2TesterCase6 {
 public:
  VectorLoadStoreMultiple2Tester_Case6()
    : VectorLoadStoreMultiple2TesterCase6(
      state_.VectorLoadStoreMultiple2_VST2_multiple_2_element_structures_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=1000
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case7
    : public VectorLoadStoreSingle1TesterCase7 {
 public:
  VectorLoadStoreSingle1Tester_Case7()
    : VectorLoadStoreSingle1TesterCase7(
      state_.VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case8
    : public VectorLoadStoreSingle2TesterCase8 {
 public:
  VectorLoadStoreSingle2Tester_Case8()
    : VectorLoadStoreSingle2TesterCase8(
      state_.VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case9
    : public VectorLoadStoreSingle3TesterCase9 {
 public:
  VectorLoadStoreSingle3Tester_Case9()
    : VectorLoadStoreSingle3TesterCase9(
      state_.VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case10
    : public VectorLoadStoreSingle4TesterCase10 {
 public:
  VectorLoadStoreSingle4Tester_Case10()
    : VectorLoadStoreSingle4TesterCase10(
      state_.VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=0x00
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case11
    : public VectorLoadStoreSingle1TesterCase11 {
 public:
  VectorLoadStoreSingle1Tester_Case11()
    : VectorLoadStoreSingle1TesterCase11(
      state_.VectorLoadStoreSingle1_VST1_single_element_from_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case12
    : public VectorLoadStoreSingle2TesterCase12 {
 public:
  VectorLoadStoreSingle2Tester_Case12()
    : VectorLoadStoreSingle2TesterCase12(
      state_.VectorLoadStoreSingle2_VST2_single_2_element_structure_from_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case13
    : public VectorLoadStoreSingle3TesterCase13 {
 public:
  VectorLoadStoreSingle3Tester_Case13()
    : VectorLoadStoreSingle3TesterCase13(
      state_.VectorLoadStoreSingle3_VST3_single_3_element_structure_from_one_lane_instance_)
  {}
};

// L(21)=0 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case14
    : public VectorLoadStoreSingle4TesterCase14 {
 public:
  VectorLoadStoreSingle4Tester_Case14()
    : VectorLoadStoreSingle4TesterCase14(
      state_.VectorLoadStoreSingle4_VST4_single_4_element_structure_form_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case15
    : public VectorLoadStoreMultiple1TesterCase15 {
 public:
  VectorLoadStoreMultiple1Tester_Case15()
    : VectorLoadStoreMultiple1TesterCase15(
      state_.VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case16
    : public VectorLoadStoreMultiple2TesterCase16 {
 public:
  VectorLoadStoreMultiple2Tester_Case16()
    : VectorLoadStoreMultiple2TesterCase16(
      state_.VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case17
    : public VectorLoadStoreMultiple1TesterCase17 {
 public:
  VectorLoadStoreMultiple1Tester_Case17()
    : VectorLoadStoreMultiple1TesterCase17(
      state_.VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       generated_baseline: VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple4Tester_Case18
    : public VectorLoadStoreMultiple4TesterCase18 {
 public:
  VectorLoadStoreMultiple4Tester_Case18()
    : VectorLoadStoreMultiple4TesterCase18(
      state_.VectorLoadStoreMultiple4_VLD4_multiple_4_element_structures_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple3Tester_Case19
    : public VectorLoadStoreMultiple3TesterCase19 {
 public:
  VectorLoadStoreMultiple3Tester_Case19()
    : VectorLoadStoreMultiple3TesterCase19(
      state_.VectorLoadStoreMultiple3_VLD3_multiple_3_element_structures_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple1Tester_Case20
    : public VectorLoadStoreMultiple1TesterCase20 {
 public:
  VectorLoadStoreMultiple1Tester_Case20()
    : VectorLoadStoreMultiple1TesterCase20(
      state_.VectorLoadStoreMultiple1_VLD1_multiple_single_elements_instance_)
  {}
};

// L(21)=1 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreMultiple2Tester_Case21
    : public VectorLoadStoreMultiple2TesterCase21 {
 public:
  VectorLoadStoreMultiple2Tester_Case21()
    : VectorLoadStoreMultiple2TesterCase21(
      state_.VectorLoadStoreMultiple2_VLD2_multiple_2_element_structures_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1000
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case22
    : public VectorLoadStoreSingle1TesterCase22 {
 public:
  VectorLoadStoreSingle1Tester_Case22()
    : VectorLoadStoreSingle1TesterCase22(
      state_.VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case23
    : public VectorLoadStoreSingle2TesterCase23 {
 public:
  VectorLoadStoreSingle2Tester_Case23()
    : VectorLoadStoreSingle2TesterCase23(
      state_.VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case24
    : public VectorLoadStoreSingle3TesterCase24 {
 public:
  VectorLoadStoreSingle3Tester_Case24()
    : VectorLoadStoreSingle3TesterCase24(
      state_.VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case25
    : public VectorLoadStoreSingle4TesterCase25 {
 public:
  VectorLoadStoreSingle4Tester_Case25()
    : VectorLoadStoreSingle4TesterCase25(
      state_.VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1100
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle1AllLanes,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1100sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if T(5)=0
//            else 2,
//       rule: VLD1_single_element_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            (size(7:6)=00 &&
//            a(4)=1) => UNDEFINED,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle1AllLanesTester_Case26
    : public VectorLoadSingle1AllLanesTesterCase26 {
 public:
  VectorLoadSingle1AllLanesTester_Case26()
    : VectorLoadSingle1AllLanesTesterCase26(
      state_.VectorLoadSingle1AllLanes_VLD1_single_element_to_all_lanes_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1101
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle2AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22), Rn(19:16), Vd(15:12), size(7:6), T(5), Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1101sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle2AllLanesTester_Case27
    : public VectorLoadSingle2AllLanesTesterCase27 {
 public:
  VectorLoadSingle2AllLanesTester_Case27()
    : VectorLoadSingle2AllLanesTesterCase27(
      state_.VectorLoadSingle2AllLanes_VLD2_single_2_element_structure_to_all_lanes_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1110
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle3AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1110sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            a(4)=1 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle3AllLanesTester_Case28
    : public VectorLoadSingle3AllLanesTesterCase28 {
 public:
  VectorLoadSingle3AllLanesTester_Case28()
    : VectorLoadSingle3AllLanesTesterCase28(
      state_.VectorLoadSingle3AllLanes_VLD3_single_3_element_structure_to_all_lanes_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=1111
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle4AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1111sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 &&
//            a(4)=0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadSingle4AllLanesTester_Case29
    : public VectorLoadSingle4AllLanesTesterCase29 {
 public:
  VectorLoadSingle4AllLanesTester_Case29()
    : VectorLoadSingle4AllLanesTesterCase29(
      state_.VectorLoadSingle4AllLanes_VLD4_single_4_element_structure_to_all_lanes_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=0x00
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle1Tester_Case30
    : public VectorLoadStoreSingle1TesterCase30 {
 public:
  VectorLoadStoreSingle1Tester_Case30()
    : VectorLoadStoreSingle1TesterCase30(
      state_.VectorLoadStoreSingle1_VLD1_single_element_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle2Tester_Case31
    : public VectorLoadStoreSingle2TesterCase31 {
 public:
  VectorLoadStoreSingle2Tester_Case31()
    : VectorLoadStoreSingle2TesterCase31(
      state_.VectorLoadStoreSingle2_VLD2_single_2_element_structure_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle3Tester_Case32
    : public VectorLoadStoreSingle3TesterCase32 {
 public:
  VectorLoadStoreSingle3Tester_Case32()
    : VectorLoadStoreSingle3TesterCase32(
      state_.VectorLoadStoreSingle3_VLD3_single_3_element_structure_to_one_lane_instance_)
  {}
};

// L(21)=1 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
class VectorLoadStoreSingle4Tester_Case33
    : public VectorLoadStoreSingle4TesterCase33 {
 public:
  VectorLoadStoreSingle4Tester_Case33()
    : VectorLoadStoreSingle4TesterCase33(
      state_.VectorLoadStoreSingle4_VLD4_single_4_element_structure_to_one_lane_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

// The following functions test each pattern specified in parse
// decoder tables.

// L(21)=0 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case0_TestCase0) {
  VectorLoadStoreMultiple1Tester_Case0 baseline_tester;
  NamedActual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1_VST1_multiple_single_elements actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case1_TestCase1) {
  VectorLoadStoreMultiple2Tester_Case1 baseline_tester;
  NamedActual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VST2_multiple_2_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case2_TestCase2) {
  VectorLoadStoreMultiple1Tester_Case2 baseline_tester;
  NamedActual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1_VST1_multiple_single_elements actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       generated_baseline: VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple4Tester_Case3_TestCase3) {
  VectorLoadStoreMultiple4Tester_Case3 baseline_tester;
  NamedActual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VST4_multiple_4_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple3Tester_Case4_TestCase4) {
  VectorLoadStoreMultiple3Tester_Case4 baseline_tester;
  NamedActual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VST3_multiple_3_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VST1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case5_TestCase5) {
  VectorLoadStoreMultiple1Tester_Case5 baseline_tester;
  NamedActual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1_VST1_multiple_single_elements actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d00nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VST2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case6_TestCase6) {
  VectorLoadStoreMultiple2Tester_Case6 baseline_tester;
  NamedActual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VST2_multiple_2_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d00nnnnddddttttssaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=1000
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case7_TestCase7) {
  VectorLoadStoreSingle1Tester_Case7 baseline_tester;
  NamedActual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1_VST1_single_element_from_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss00aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case8_TestCase8) {
  VectorLoadStoreSingle2Tester_Case8 baseline_tester;
  NamedActual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1_VST2_single_2_element_structure_from_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss01aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case9_TestCase9) {
  VectorLoadStoreSingle3Tester_Case9 baseline_tester;
  NamedActual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1_VST3_single_3_element_structure_from_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss10aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case10_TestCase10) {
  VectorLoadStoreSingle4Tester_Case10 baseline_tester;
  NamedActual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1_VST4_single_4_element_structure_form_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss11aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=0x00
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST1_single_element_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case11_TestCase11) {
  VectorLoadStoreSingle1Tester_Case11 baseline_tester;
  NamedActual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1_VST1_single_element_from_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss00aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST2_single_2_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case12_TestCase12) {
  VectorLoadStoreSingle2Tester_Case12 baseline_tester;
  NamedActual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1_VST2_single_2_element_structure_from_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss01aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST3_single_3_element_structure_from_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case13_TestCase13) {
  VectorLoadStoreSingle3Tester_Case13 baseline_tester;
  NamedActual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1_VST3_single_3_element_structure_from_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss10aaaammmm");
}

// L(21)=0 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d00nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VST4_single_4_element_structure_form_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case14_TestCase14) {
  VectorLoadStoreSingle4Tester_Case14 baseline_tester;
  NamedActual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1_VST4_single_4_element_structure_form_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d00nnnnddddss11aaaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=0010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case15_TestCase15) {
  VectorLoadStoreMultiple1Tester_Case15 baseline_tester;
  NamedActual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1_VLD1_multiple_single_elements actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=0011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case16_TestCase16) {
  VectorLoadStoreMultiple2Tester_Case16 baseline_tester;
  NamedActual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VLD2_multiple_2_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case17_TestCase17) {
  VectorLoadStoreMultiple1Tester_Case17 baseline_tester;
  NamedActual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1_VLD1_multiple_single_elements actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=000x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreMultiple4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         Rm(3:0)],
//       generated_baseline: VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_multiple_4_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         not type in bitset {'0000', '0001'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple4Tester_Case18_TestCase18) {
  VectorLoadStoreMultiple4Tester_Case18 baseline_tester;
  NamedActual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VLD4_multiple_4_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=010x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=0100
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_multiple_3_element_structures,
//       safety: [size(7:6)=11 ||
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0100', '0101'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple3Tester_Case19_TestCase19) {
  VectorLoadStoreMultiple3Tester_Case19 baseline_tester;
  NamedActual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VLD3_multiple_3_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=011x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple1,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type(11:8)=0111
//            else 2
//            if type(11:8)=1010
//            else 3
//            if type(11:8)=0110
//            else 4
//            if type(11:8)=0010
//            else 0,
//       rule: VLD1_multiple_single_elements,
//       safety: [type(11:8)=0111 &&
//            align(1)=1 => UNDEFINED,
//         type(11:8)=1010 &&
//            align(5:4)=11 => UNDEFINED,
//         type(11:8)=0110 &&
//            align(1)=1 => UNDEFINED,
//         not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple1Tester_Case20_TestCase20) {
  VectorLoadStoreMultiple1Tester_Case20 baseline_tester;
  NamedActual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1_VLD1_multiple_single_elements actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=0 & B(11:8)=100x
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//       align: align(5:4),
//       base: n,
//       baseline: VectorLoadStoreMultiple2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         type(11:8),
//         size(7:6),
//         align(5:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//       inc: 1
//            if type(11:8)=1000
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101000d10nnnnddddttttssaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if type in bitset {'1000', '1001'}
//            else 2,
//       rule: VLD2_multiple_2_element_structures,
//       safety: [size(7:6)=11 => UNDEFINED,
//         type in bitset {'1000', '1001'} &&
//            align(5:4)=11 => UNDEFINED,
//         not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//         n  ==
//               Pc ||
//            d2 + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       type: type(11:8),
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreMultiple2Tester_Case21_TestCase21) {
  VectorLoadStoreMultiple2Tester_Case21 baseline_tester;
  NamedActual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1_VLD2_multiple_2_element_structures actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101000d10nnnnddddttttssaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1000
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case22_TestCase22) {
  VectorLoadStoreSingle1Tester_Case22 baseline_tester;
  NamedActual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1_VLD1_single_element_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss00aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1001
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case23_TestCase23) {
  VectorLoadStoreSingle2Tester_Case23 baseline_tester;
  NamedActual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1_VLD2_single_2_element_structure_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss01aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1010
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case24_TestCase24) {
  VectorLoadStoreSingle3Tester_Case24 baseline_tester;
  NamedActual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1_VLD3_single_3_element_structure_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss10aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1011
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case25_TestCase25) {
  VectorLoadStoreSingle4Tester_Case25 baseline_tester;
  NamedActual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1_VLD4_single_4_element_structure_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss11aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1100
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle1AllLanes,
//       constraints: ,
//       d: D:Vd,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1100sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       regs: 1
//            if T(5)=0
//            else 2,
//       rule: VLD1_single_element_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            (size(7:6)=00 &&
//            a(4)=1) => UNDEFINED,
//         n  ==
//               Pc ||
//            d + regs  >
//               32 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle1AllLanesTester_Case26_TestCase26) {
  VectorLoadSingle1AllLanesTester_Case26 baseline_tester;
  NamedActual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1_VLD1_single_element_to_all_lanes actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnndddd1100sstammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1101
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle2AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22), Rn(19:16), Vd(15:12), size(7:6), T(5), Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1101sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle2AllLanesTester_Case27_TestCase27) {
  VectorLoadSingle2AllLanesTester_Case27 baseline_tester;
  NamedActual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1_VLD2_single_2_element_structure_to_all_lanes actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnndddd1101sstammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1110
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle3AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1110sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 ||
//            a(4)=1 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle3AllLanesTester_Case28_TestCase28) {
  VectorLoadSingle3AllLanesTester_Case28 baseline_tester;
  NamedActual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1_VLD3_single_3_element_structure_to_all_lanes actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnndddd1110sstammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=1111
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       T: T(5),
//       Vd: Vd(15:12),
//       a: a(4),
//       actual: Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1,
//       base: n,
//       baseline: VectorLoadSingle4AllLanes,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(7:6),
//         T(5),
//         a(4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0,
//       inc: 1
//            if T(5)=0
//            else 2,
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnndddd1111sstammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_all_lanes,
//       safety: [size(7:6)=11 &&
//            a(4)=0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(7:6),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadSingle4AllLanesTester_Case29_TestCase29) {
  VectorLoadSingle4AllLanesTester_Case29 baseline_tester;
  NamedActual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1_VLD4_single_4_element_structure_to_all_lanes actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnndddd1111sstammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=0x00
//    = {None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle1,
//       constraints: ,
//       defs: {base}
//            if wback
//            else {},
//       fields: [Rn(19:16), size(11:10), index_align(7:4), Rm(3:0)],
//       generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss00aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD1_single_element_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(1)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(2)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 &&
//            index_align(1:0)=~11 => UNDEFINED,
//         n  ==
//               Pc => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle1Tester_Case30_TestCase30) {
  VectorLoadStoreSingle1Tester_Case30 baseline_tester;
  NamedActual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1_VLD1_single_element_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss00aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=0x01
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle2,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss01aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD2_single_2_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1)=~0 => UNDEFINED,
//         n  ==
//               Pc ||
//            d2  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle2Tester_Case31_TestCase31) {
  VectorLoadStoreSingle2Tester_Case31 baseline_tester;
  NamedActual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1_VLD2_single_2_element_structure_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss01aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=0x10
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle3,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss10aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD3_single_3_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=00 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=01 &&
//            index_align(0)=~0 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=~00 => UNDEFINED,
//         n  ==
//               Pc ||
//            d3  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle3Tester_Case32_TestCase32) {
  VectorLoadStoreSingle3Tester_Case32 baseline_tester;
  NamedActual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1_VLD3_single_3_element_structure_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss10aaaammmm");
}

// L(21)=1 & A(23)=1 & B(11:8)=0x11
//    = {D: D(22),
//       None: 32,
//       Pc: 15,
//       Rm: Rm(3:0),
//       Rn: Rn(19:16),
//       Sp: 13,
//       Vd: Vd(15:12),
//       actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//       base: n,
//       baseline: VectorLoadStoreSingle4,
//       constraints: ,
//       d: D:Vd,
//       d2: d + inc,
//       d3: d2 + inc,
//       d4: d3 + inc,
//       defs: {base}
//            if wback
//            else {},
//       fields: [D(22),
//         Rn(19:16),
//         Vd(15:12),
//         size(11:10),
//         index_align(7:4),
//         Rm(3:0)],
//       generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//       inc: 1
//            if size(11:10)=00
//            else (1
//            if index_align(1)=0
//            else 2)
//            if size(11:10)=01
//            else (1
//            if index_align(2)=0
//            else 2)
//            if size(11:10)=10
//            else 0,
//       index_align: index_align(7:4),
//       m: Rm,
//       n: Rn,
//       pattern: 111101001d10nnnnddddss11aaaammmm,
//       register_index: (m  !=
//               Pc &&
//            m  !=
//               Sp),
//       rule: VLD4_single_4_element_structure_to_one_lane,
//       safety: [size(11:10)=11 => UNDEFINED,
//         size(11:10)=10 &&
//            index_align(1:0)=11 => UNDEFINED,
//         n  ==
//               Pc ||
//            d4  >
//               31 => UNPREDICTABLE],
//       size: size(11:10),
//       small_imm_base_wb: wback &&
//            not register_index,
//       uses: {m
//            if wback
//            else None, n},
//       wback: (m  !=
//               Pc)}
TEST_F(Arm32DecoderStateTests,
       VectorLoadStoreSingle4Tester_Case33_TestCase33) {
  VectorLoadStoreSingle4Tester_Case33 baseline_tester;
  NamedActual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1_VLD4_single_4_element_structure_to_one_lane actual;
  ActualVsBaselineTester a_vs_b_tester(actual, baseline_tester);
  a_vs_b_tester.Test("111101001d10nnnnddddss11aaaammmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
