/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#include "native_client/src/trusted/validator_arm/gen/arm32_decode_baselines.h"
#include "native_client/src/trusted/validator_arm/inst_classes.h"

namespace nacl_arm_dec {

// VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(5:4)=00
//         else 4 << align,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple1,
//    constraints: ,
//    d: D:Vd,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d10nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    regs: 1
//         if type(11:8)=0111
//         else 2
//         if type(11:8)=1010
//         else 3
//         if type(11:8)=0110
//         else 4
//         if type(11:8)=0010
//         else 0,
//    rule: VLD1_multiple_single_elements,
//    safety: [type(11:8)=0111 &&
//         align(1)=1 => UNDEFINED,
//      type(11:8)=1010 &&
//         align(5:4)=11 => UNDEFINED,
//      type(11:8)=0110 &&
//         align(1)=1 => UNDEFINED,
//      not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:8)=0111 &&
  //       inst(5:4)(1)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002))
    return UNDEFINED;

  // inst(11:8)=1010 &&
  //       inst(5:4)=11 => UNDEFINED
  if (((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030))
    return UNDEFINED;

  // inst(11:8)=0110 &&
  //       inst(5:4)(1)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002))
    return UNDEFINED;

  // not inst(11:8)=0111 ||
  //       inst(11:8)=1010 ||
  //       inst(11:8)=0110 ||
  //       inst(11:8)=0010 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       32  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=0111
  //       else 2
  //       if inst(11:8)=1010
  //       else 3
  //       if inst(11:8)=0110
  //       else 4
  //       if inst(11:8)=0010
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
//    actual: Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1,
//    alignment: 1
//         if a(4)=0
//         else ebytes,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadSingle1AllLanes,
//    constraints: ,
//    d: D:Vd,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(7:6),
//      T(5),
//      a(4),
//      Rm(3:0)],
//    generated_baseline: VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0,
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnndddd1100sstammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    regs: 1
//         if T(5)=0
//         else 2,
//    rule: VLD1_single_element_to_all_lanes,
//    safety: [size(7:6)=11 ||
//         (size(7:6)=00 &&
//         a(4)=1) => UNDEFINED,
//      n  ==
//            Pc ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 ||
  //       (inst(7:6)=00 &&
  //       inst(4)=1) => UNDEFINED
  if (((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((((inst.Bits() & 0x000000C0)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000010)  ==
          0x00000010))))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       32  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(5)=0
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//    alignment: 1
//         if size(11:10)=00
//         else (1
//         if index_align(0)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(1:0)=00
//         else 4)
//         if size(11:10)=10
//         else 0,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle1,
//    constraints: ,
//    d: D:Vd,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnnddddss00aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD1_single_element_to_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=00 &&
//         index_align(0)=~0 => UNDEFINED,
//      size(11:10)=01 &&
//         index_align(1)=~0 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(2)=~0 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1:0)=~00 &&
//         index_align(1:0)=~11 => UNDEFINED,
//      n  ==
//            Pc => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=00 &&
  //       inst(7:4)(0)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=01 &&
  //       inst(7:4)(1)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(2)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1:0)=~00 &&
  //       inst(7:4)(1:0)=~11 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000003))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(5:4)=00
//         else 4 << align,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple2,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//    inc: 1
//         if type(11:8)=1000
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d10nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    regs: 1
//         if type in bitset {'1000', '1001'}
//         else 2,
//    rule: VLD2_multiple_2_element_structures,
//    safety: [size(7:6)=11 => UNDEFINED,
//      type in bitset {'1000', '1001'} &&
//         align(5:4)=11 => UNDEFINED,
//      not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d2 + regs  >
//            32 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 => UNDEFINED
  if ((inst.Bits() & 0x000000C0)  ==
          0x000000C0)
    return UNDEFINED;

  // inst(11:8)=1000 ||
  //       inst(11:8)=1001 &&
  //       inst(5:4)=11 => UNDEFINED
  if ((((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030))
    return UNDEFINED;

  // not inst(11:8)=1000 ||
  //       inst(11:8)=1001 ||
  //       inst(11:8)=0011 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000300)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       32  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=1000
  //       else 2 + 1
  //       if inst(11:8)=1000 ||
  //       inst(11:8)=1001
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000800
       ? 1
       : 2) + (((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)
       ? 1
       : 2)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
//    actual: Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1,
//    alignment: 1
//         if a(4)=0
//         else 2 * ebytes,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadSingle2AllLanes,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(7:6),
//      T(5),
//      a(4),
//      Rm(3:0)],
//    generated_baseline: VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0,
//    inc: 1
//         if T(5)=0
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnndddd1101sstammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD2_single_2_element_structure_to_all_lanes,
//    safety: [size(7:6)=11 => UNDEFINED,
//      n  ==
//            Pc ||
//         d2  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 => UNDEFINED
  if ((inst.Bits() & 0x000000C0)  ==
          0x000000C0)
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(5)=0
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//    alignment: (1
//         if index_align(0)=0
//         else 2)
//         if size(11:10)=00
//         else (1
//         if index_align(0)=0
//         else 4)
//         if size(11:10)=01
//         else (1
//         if index_align(0)=0
//         else 8)
//         if size(11:10)=10
//         else 0,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle2,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnnddddss01aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD2_single_2_element_structure_to_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1)=~0 => UNDEFINED,
//      n  ==
//            Pc ||
//         d2  >
//            31 => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(0)=0
//         else 8,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple3,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//    inc: 1
//         if type(11:8)=0100
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d10nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD3_multiple_3_element_structures,
//    safety: [size(7:6)=11 ||
//         align(1)=1 => UNDEFINED,
//      not type in bitset {'0100', '0101'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d3  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 ||
  //       inst(5:4)(1)=1 => UNDEFINED
  if (((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002))
    return UNDEFINED;

  // not inst(11:8)=0100 ||
  //       inst(11:8)=0101 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000400) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000500)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=0100
  //       else 2 + 1
  //       if inst(11:8)=0100
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
//    actual: Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1,
//    alignment: 1,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadSingle3AllLanes,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(7:6),
//      T(5),
//      a(4),
//      Rm(3:0)],
//    generated_baseline: VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0,
//    inc: 1
//         if T(5)=0
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnndddd1110sstammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD3_single_3_element_structure_to_all_lanes,
//    safety: [size(7:6)=11 ||
//         a(4)=1 => UNDEFINED,
//      n  ==
//            Pc ||
//         d3  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 ||
  //       inst(4)=1 => UNDEFINED
  if (((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((inst.Bits() & 0x00000010)  ==
          0x00000010))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(5)=0
  //       else 2 + 1
  //       if inst(5)=0
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//    alignment: 1,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle3,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnnddddss10aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD3_single_3_element_structure_to_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=00 &&
//         index_align(0)=~0 => UNDEFINED,
//      size(11:10)=01 &&
//         index_align(0)=~0 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1:0)=~00 => UNDEFINED,
//      n  ==
//            Pc ||
//         d3  >
//            31 => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=00 &&
  //       inst(7:4)(0)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=01 &&
  //       inst(7:4)(0)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1:0)=~00 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(5:4)=00
//         else 4 << align,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple4,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    d4: d3 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0,
//    inc: 1
//         if type(11:8)=0000
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d10nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD4_multiple_4_element_structures,
//    safety: [size(7:6)=11 => UNDEFINED,
//      not type in bitset {'0000', '0001'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d4  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 => UNDEFINED
  if ((inst.Bits() & 0x000000C0)  ==
          0x000000C0)
    return UNDEFINED;

  // not inst(11:8)=0000 ||
  //       inst(11:8)=0001 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000100)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=0000
  //       else 2 + 1
  //       if inst(11:8)=0000
  //       else 2 + 1
  //       if inst(11:8)=0000
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
//    actual: Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1,
//    alignment: 16
//         if size(7:6)=11
//         else (1
//         if a(4)=0
//         else 8)
//         if size(7:6)=10
//         else (1
//         if a(4)=0
//         else 4 * ebytes),
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadSingle4AllLanes,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    d4: d3 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(7:6),
//      T(5),
//      a(4),
//      Rm(3:0)],
//    generated_baseline: VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0,
//    inc: 1
//         if T(5)=0
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnndddd1111sstammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD4_single_4_element_structure_to_all_lanes,
//    safety: [size(7:6)=11 &&
//         a(4)=0 => UNDEFINED,
//      n  ==
//            Pc ||
//         d4  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 &&
  //       inst(4)=0 => UNDEFINED
  if (((inst.Bits() & 0x000000C0)  ==
          0x000000C0) &&
       ((inst.Bits() & 0x00000010)  ==
          0x00000000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(5)=0
  //       else 2 + 1
  //       if inst(5)=0
  //       else 2 + 1
  //       if inst(5)=0
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000020)  ==
          0x00000000
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//    alignment: (1
//         if index_align(0)=0
//         else 4)
//         if size(11:10)=00
//         else (1
//         if index_align(0)=0
//         else 8)
//         if size(11:10)=01
//         else (1
//         if index_align(1:0)=00
//         else 4 << index_align(1:0))
//         if size(11:10)=10
//         else 0,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle4,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    d4: d3 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d10nnnnddddss11aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VLD4_single_4_element_structure_to_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1:0)=11 => UNDEFINED,
//      n  ==
//            Pc ||
//         d4  >
//            31 => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1:0)=11 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  ==
          0x00000003))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0:
//
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
//    actual: Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: VFPv2,
//    base: Rn,
//    baseline: LoadVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {Rn
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      D(22),
//      W(21),
//      Rn(19:16),
//      Vd(15:12),
//      imm8(7:0)],
//    generated_baseline: VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    is_literal_load: Rn  ==
//            Pc,
//    n: Rn,
//    pattern: cccc110pudw1nnnndddd1010iiiiiiii,
//    regs: imm8,
//    rule: VLDM,
//    safety: [P(24)=0 &&
//         U(23)=0 &&
//         W(21)=0 => DECODER_ERROR,
//      P(24)=1 &&
//         W(21)=0 => DECODER_ERROR,
//      P  ==
//            U &&
//         W(21)=1 => UNDEFINED,
//      n  ==
//            Pc &&
//         wback => UNPREDICTABLE,
//      P(24)=0 &&
//         U(23)=1 &&
//         W(21)=1 &&
//         Rn  ==
//            Sp => DECODER_ERROR,
//      regs  ==
//            0 ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    single_regs: true,
//    small_imm_base_wb: wback,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
Register VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

bool VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: '15  ==
  //          inst(19:16)'
  return ((((inst.Bits() & 0x000F0000) >> 16)) == (15));
}

SafetyLevel VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(23)=0 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(24)=1 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(23)  ==
  //          inst(24) &&
  //       inst(21)=1 => UNDEFINED
  if ((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) &&
  //       inst(21)=1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // inst(24)=0 &&
  //       inst(23)=1 &&
  //       inst(21)=1 &&
  //       13  ==
  //          inst(19:16) => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13))))
    return DECODER_ERROR;

  // 0  ==
  //          inst(7:0) ||
  //       32  <=
  //          inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0:
//
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
//    actual: Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Rn,
//    baseline: LoadVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {Rn
//         if wback
//         else None},
//    false: false,
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      D(22),
//      W(21),
//      Rn(19:16),
//      Vd(15:12),
//      imm8(7:0)],
//    generated_baseline: VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    is_literal_load: Rn  ==
//            Pc,
//    n: Rn,
//    pattern: cccc110pudw1nnnndddd1011iiiiiiii,
//    regs: imm8 / 2,
//    rule: VLDM,
//    safety: [P(24)=0 &&
//         U(23)=0 &&
//         W(21)=0 => DECODER_ERROR,
//      P(24)=1 &&
//         W(21)=0 => DECODER_ERROR,
//      P  ==
//            U &&
//         W(21)=1 => UNDEFINED,
//      n  ==
//            Pc &&
//         wback => UNPREDICTABLE,
//      P(24)=0 &&
//         U(23)=1 &&
//         W(21)=1 &&
//         Rn  ==
//            Sp => DECODER_ERROR,
//      regs  ==
//            0 ||
//         regs  >
//            16 ||
//         d + regs  >
//            32 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         d + regs  >
//            16 => UNPREDICTABLE,
//      imm8(0)  ==
//            1 => DEPRECATED],
//    single_regs: false,
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: W(21)=1}
Register VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

bool VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: '15  ==
  //          inst(19:16)'
  return ((((inst.Bits() & 0x000F0000) >> 16)) == (15));
}

SafetyLevel VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(23)=0 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(24)=1 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(23)  ==
  //          inst(24) &&
  //       inst(21)=1 => UNDEFINED
  if ((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) &&
  //       inst(21)=1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // inst(24)=0 &&
  //       inst(23)=1 &&
  //       inst(21)=1 &&
  //       13  ==
  //          inst(19:16) => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13))))
    return DECODER_ERROR;

  // 0  ==
  //          inst(7:0) / 2 ||
  //       16  <=
  //          inst(7:0) / 2 ||
  //       32  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32))))
    return UNPREDICTABLE;

  // VFPSmallRegisterBank() &&
  //       16  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if ((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16))))
    return UNPREDICTABLE;

  // 1  ==
  //          inst(7:0)(0) => DEPRECATED
  if (((((inst.Bits() & 0x000000FF) & 0x00000001)) == (1)))
    return DEPRECATED;

  return MAY_BE_SAFE;
}


bool VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0:
//
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    actual: Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: VFPv2,
//    base: Rn,
//    baseline: LoadVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    generated_baseline: VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    is_literal_load: Rn  ==
//            Pc,
//    n: Rn,
//    pattern: cccc1101ud01nnnndddd1010iiiiiiii,
//    rule: VLDR,
//    single_reg: true,
//    true: true,
//    uses: {Rn}}
Register VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

bool VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: '15  ==
  //          inst(19:16)'
  return ((((inst.Bits() & 0x000F0000) >> 16)) == (15));
}

SafetyLevel VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  return MAY_BE_SAFE;
}


RegisterList VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0:
//
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    actual: Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Rn,
//    baseline: LoadVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    false: false,
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    generated_baseline: VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    is_literal_load: Rn  ==
//            Pc,
//    n: Rn,
//    pattern: cccc1101ud01nnnndddd1011iiiiiiii,
//    rule: VLDR,
//    single_reg: false,
//    uses: {Rn}}
Register VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

bool VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: '15  ==
  //          inst(19:16)'
  return ((((inst.Bits() & 0x000F0000) >> 16)) == (15));
}

SafetyLevel VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  return MAY_BE_SAFE;
}


RegisterList VLDR_cccc1101ud01nnnndddd1011iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0110nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMAX,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMAX_1111001u0dssnnnndddd0110nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dssnnnndddd1111nqm0mmmm,
//    rule: VMAX_floating_point,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMAX_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0110nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMIN,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMIN_1111001u0dssnnnndddd0110nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dssnnnndddd1111nqm0mmmm,
//    rule: VMIN_floating_point,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMIN_floating_point_111100100dssnnnndddd1111nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLAL_VMLSL_integer_A2_1111001u1dssnnnndddd10p0n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLAL_VMLSL_integer_A2_1111001u1dssnnnndddd10p0n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd10p0n0m0mmmm,
//    rule: VMLAL_VMLSL_integer_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMLAL_VMLSL_integer_A2_1111001u1dssnnnndddd10p0n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLAL_VMLSL_integer_A2_1111001u1dssnnnndddd10p0n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(15:12)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLAL_VMLSL_integer_A2_1111001u1dssnnnndddd10p0n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001u1dssnnnndddd0p10n1m0mmmm,
//    regs: 1,
//    rule: VMLAL_by_scalar_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: Q(24)=1,
//    uses: {}}
RegisterList VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1,
//    add: op(6)=0,
//    advsimd: false,
//    arch: VFPv2,
//    baseline: VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd
//         if dp_operation
//         else Vd:D,
//    defs: {},
//    dp_operation: sz(8)=1,
//    false: false,
//    fields: [cond(31:28),
//      D(22),
//      Vn(19:16),
//      Vd(15:12),
//      sz(8),
//      N(7),
//      op(6),
//      M(5),
//      Vm(3:0)],
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    op: op(6),
//    pattern: cccc11100d00nnnndddd101snom0mmmm,
//    rule: VMLA_VMLS_floating_point,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
RegisterList VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(31:28)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0xF0000000)  ==
          0xF0000000)
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VMLA_VMLS_floating_point_cccc11100d00nnnndddd101snom0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_F32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0,
//    index: M,
//    m: Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VMLA_by_scalar_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         size(21:20)=01) => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(21:20)=01) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00100000)))
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VMLA_by_scalar_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00000000)
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//    rule: VMLA_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLA_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMLA_integer_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLA_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLSL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLSL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001u1dssnnnndddd0p10n1m0mmmm,
//    regs: 1,
//    rule: VMLSL_by_scalar_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: Q(24)=1,
//    uses: {}}
RegisterList VMLSL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLSL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLSL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_F32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0,
//    index: M,
//    m: Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VMLS_by_scalar_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         size(21:20)=01) => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(21:20)=01) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00100000)))
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd0p0fn1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VMLS_by_scalar_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00000000)
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLS_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//    rule: VMLS_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLS_floating_point_A1_111100100dpsnnnndddd1101nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1001nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMLS_integer_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMLS_integer_A1_1111001u0dssnnnndddd1001nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMOVN_111100111d11ss10dddd001000m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V16_32_64N,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMOVN_111100111d11ss10dddd001000m0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss10dddd001000m0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMOVN,
//    safety: [size(19:18)=11 => UNDEFINED, Vm(0)=1 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VMOVN_111100111d11ss10dddd001000m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMOVN_111100111d11ss10dddd001000m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMOVN_111100111d11ss10dddd001000m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0:
//
//   {D: D(7),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Vd: Vd(19:16),
//    actual: Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1,
//    advsimd: sel in bitset {'1xxx', '0xx1'},
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: MoveVfpRegisterOpWithTypeSel,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    esize: 8
//         if opc1:opc2(3:0)=1xxx
//         else 16
//         if opc1:opc2(3:0)=0xx1
//         else 32
//         if opc1:opc2(3:0)=0x00
//         else 0,
//    fields: [cond(31:28),
//      opc1(22:21),
//      Vd(19:16),
//      Rt(15:12),
//      D(7),
//      opc2(6:5)],
//    generated_baseline: VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0,
//    index: opc1(0):opc2
//         if opc1:opc2(3:0)=1xxx
//         else opc1(0):opc2(1)
//         if opc1:opc2(3:0)=0xx1
//         else opc1(0)
//         if opc1:opc2(3:0)=0x00
//         else 0,
//    opc1: opc1(22:21),
//    opc2: opc2(6:5),
//    pattern: cccc11100ii0ddddtttt1011dii10000,
//    rule: VMOV_ARM_core_register_to_scalar,
//    safety: [opc1:opc2(3:0)=0x10 => UNDEFINED,
//      t  ==
//            Pc => UNPREDICTABLE],
//    sel: opc1:opc2,
//    t: Rt,
//    uses: {Rt}}
RegisterList VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(22:21):inst(6:5)(3:0)=0x10 => UNDEFINED
  if (((((((inst.Bits() & 0x00600000) >> 21)) << 2) | ((inst.Bits() & 0x00000060) >> 5)) & 0x0000000B)  ==
          0x00000002)
    return UNDEFINED;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0:
//
//   {N: N(7),
//    None: 32,
//    Pc: 15,
//    Rt: Rt(15:12),
//    Vn: Vn(19:16),
//    actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//    arch: VFPv2,
//    baseline: MoveVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt
//         if to_arm_register
//         else None},
//    fields: [cond(31:28), op(20), Vn(19:16), Rt(15:12), N(7)],
//    generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0,
//    n: Vn:N,
//    op: op(20),
//    pattern: cccc1110000onnnntttt1010n0010000,
//    rule: VMOV_between_ARM_core_register_and_single_precision_register,
//    safety: [t  ==
//            Pc => UNPREDICTABLE],
//    t: Rt,
//    to_arm_register: op(20)=1,
//    uses: {Rt
//         if not to_arm_register
//         else None}}
RegisterList VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? ((inst.Bits() & 0x0000F000) >> 12)
       : 32)));
}

SafetyLevel VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)
  //       if not inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register((!((inst.Bits() & 0x00100000)  ==
          0x00100000)
       ? ((inst.Bits() & 0x0000F000) >> 12)
       : 32)));
}

// VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0:
//
//   {N: N(7),
//    None: 32,
//    Pc: 15,
//    Rt: Rt(15:12),
//    Vn: Vn(19:16),
//    actual: Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1,
//    arch: VFPv2,
//    baseline: MoveVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt
//         if to_arm_register
//         else None},
//    fields: [cond(31:28), op(20), Vn(19:16), Rt(15:12), N(7)],
//    generated_baseline: VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0,
//    n: Vn:N,
//    op: op(20),
//    pattern: cccc1110000xnnnntttt1010n0010000,
//    rule: VMOV_between_ARM_core_register_and_single_precision_register,
//    safety: [t  ==
//            Pc => UNPREDICTABLE],
//    t: Rt,
//    to_arm_register: op(20)=1,
//    uses: {Rt
//         if not to_arm_register
//         else None}}
RegisterList VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? ((inst.Bits() & 0x0000F000) >> 12)
       : 32)));
}

SafetyLevel VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000xnnnntttt1010n0010000_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)
  //       if not inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register((!((inst.Bits() & 0x00100000)  ==
          0x00100000)
       ? ((inst.Bits() & 0x0000F000) >> 12)
       : 32)));
}

// VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0:
//
//   {M: M(5),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Rt2: Rt2(19:16),
//    Vm: Vm(3:0),
//    actual: Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: MoveDoubleVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2}
//         if to_arm_registers
//         else {},
//    fields: [cond(31:28), op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
//    generated_baseline: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0,
//    m: M:Vm,
//    op: op(20),
//    pattern: cccc1100010otttttttt101100m1mmmm,
//    rule: VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register,
//    safety: [Pc in {t, t2} => UNPREDICTABLE,
//      to_arm_registers &&
//         t  ==
//            t2 => UNPREDICTABLE],
//    t: Rt,
//    t2: Rt2,
//    to_arm_registers: op(20)=1,
//    uses: {}
//         if to_arm_registers
//         else {Rt, Rt2}}
RegisterList VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)}
  //       if inst(20)=1
  //       else {}'
  return ((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  // inst(20)=1 &&
  //       inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((inst.Bits() & 0x00100000)  ==
          0x00100000) &&
       (((((inst.Bits() & 0x0000F000) >> 12)) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}
  //       if inst(20)=1
  //       else {inst(15:12), inst(19:16)}'
  return ((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? RegisterList()
       : RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))));
}

// VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0:
//
//   {M: M(5),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Rt2: Rt2(19:16),
//    Vm: Vm(3:0),
//    actual: Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1,
//    arch: ['VFPv2'],
//    baseline: MoveDoubleVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2}
//         if to_arm_registers
//         else {},
//    fields: [cond(31:28), op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
//    generated_baseline: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0,
//    m: Vm:M,
//    op: op(20),
//    pattern: cccc1100010otttttttt101000m1mmmm,
//    rule: VMOV_between_two_ARM_core_registers_and_two_single_precision_registers,
//    safety: [Pc in {t, t2} ||
//         m  ==
//            31 => UNPREDICTABLE,
//      to_arm_registers &&
//         t  ==
//            t2 => UNPREDICTABLE],
//    t: Rt,
//    t2: Rt2,
//    to_arm_registers: op(20)=1,
//    uses: {}
//         if to_arm_registers
//         else {Rt, Rt2}}
RegisterList VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)}
  //       if inst(20)=1
  //       else {}'
  return ((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) ||
  //       31  ==
  //          inst(3:0):inst(5) => UNPREDICTABLE
  if (((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16))))) ||
       (((((((inst.Bits() & 0x0000000F)) << 1) | ((inst.Bits() & 0x00000020) >> 5))) == (31))))
    return UNPREDICTABLE;

  // inst(20)=1 &&
  //       inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((inst.Bits() & 0x00100000)  ==
          0x00100000) &&
       (((((inst.Bits() & 0x0000F000) >> 12)) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}
  //       if inst(20)=1
  //       else {inst(15:12), inst(19:16)}'
  return ((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? RegisterList()
       : RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))));
}

// VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0:
//
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    actual: Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_MOV,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    false: false,
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
//    generated_baseline: VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0,
//    i: i(24),
//    imm3: imm3(18:16),
//    imm4: imm4(3:0),
//    imm64: AdvSIMDExpandImm(op, cmode, i:imm3:imm4),
//    op: op(5),
//    pattern: 1111001m1d000mmmddddcccc0qp1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMOV_immediate_A1,
//    safety: [op(5)=0 &&
//         cmode(0)=1 &&
//         cmode(3:2)=~11 => DECODER_ERROR,
//      op(5)=1 &&
//         cmode(11:8)=~1110 => DECODER_ERROR,
//      Q(6)=1 &&
//         Vd(0)=1 => UNDEFINED],
//    single_register: false,
//    uses: {}}
RegisterList VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(5)=0 &&
  //       inst(11:8)(0)=1 &&
  //       inst(11:8)(3:2)=~11 => DECODER_ERROR
  if (((inst.Bits() & 0x00000020)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001)  ==
          0x00000001) &&
       ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C)  !=
          0x0000000C))
    return DECODER_ERROR;

  // inst(5)=1 &&
  //       inst(11:8)=~1110 => DECODER_ERROR
  if (((inst.Bits() & 0x00000020)  ==
          0x00000020) &&
       ((inst.Bits() & 0x00000F00)  !=
          0x00000E00))
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0:
//
//   {D: D(22),
//    Vd: Vd(15:12),
//    actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//    advsimd: false,
//    arch: VFPv3,
//    baseline: CondVfpOp,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D
//         if sz(8)=0
//         else D:Vd,
//    defs: {},
//    false: false,
//    fields: [cond(31:28),
//      D(22),
//      imm4H(19:16),
//      Vd(15:12),
//      sz(8),
//      imm4L(3:0)],
//    generated_baseline: VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0,
//    imm32: VFPExpandImm(imm4H:imm4L, 32),
//    imm4H: imm4H(19:16),
//    imm4L: imm4L(3:0),
//    imm64: VFPExpandImm(imm4H:imm4L, 64),
//    pattern: cccc11101d11iiiidddd101s0000iiii,
//    regs: 1,
//    rule: VMOV_immediate,
//    safety: [true => MAY_BE_SAFE],
//    single_register: sz(8)=0,
//    sz: sz(8),
//    true: true,
//    uses: {}}
RegisterList VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => MAY_BE_SAFE
  if (true)
    return MAY_BE_SAFE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_immediate_cccc11101d11iiiidddd101s0000iiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//    advsimd: false,
//    arch: VFPv2,
//    baseline: CondVfpOp,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D
//         if sz(8)=0
//         else D:Vd,
//    defs: {},
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), sz(8), M(5), Vm(3:0)],
//    generated_baseline: VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0,
//    m: Vm:D
//         if sz(8)=0
//         else M:Vm,
//    pattern: cccc11101d110000dddd101s01m0mmmm,
//    regs: 1,
//    rule: VMOV_register,
//    safety: [true => MAY_BE_SAFE],
//    single_register: sz(8)=0,
//    sz: sz(8),
//    true: true,
//    uses: {}}
RegisterList VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => MAY_BE_SAFE
  if (true)
    return MAY_BE_SAFE;

  return MAY_BE_SAFE;
}


RegisterList VMOV_register_cccc11101d110000dddd101s01m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMRS_cccc111011110001tttt101000010000_case_0:
//
//   {NZCV: 16,
//    Pc: 15,
//    Rt: Rt(15:12),
//    actual: Actual_VMRS_cccc111011110001tttt101000010000_case_1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: VfpMrsOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if t  ==
//            Pc
//         else Rt},
//    fields: [cond(31:28), Rt(15:12)],
//    generated_baseline: VMRS_cccc111011110001tttt101000010000_case_0,
//    pattern: cccc111011110001tttt101000010000,
//    rule: VMRS,
//    t: Rt}
RegisterList VMRS_cccc111011110001tttt101000010000_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{16
  //       if 15  ==
  //          inst(15:12)
  //       else inst(15:12)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000F000) >> 12)) == (15))
       ? 16
       : ((inst.Bits() & 0x0000F000) >> 12))));
}

SafetyLevel VMRS_cccc111011110001tttt101000010000_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  return MAY_BE_SAFE;
}


// VMSR_cccc111011100001tttt101000010000_case_0:
//
//   {Pc: 15,
//    Rt: Rt(15:12),
//    actual: Actual_VMSR_cccc111011100001tttt101000010000_case_1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: VfpUsesRegOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28), Rt(15:12)],
//    generated_baseline: VMSR_cccc111011100001tttt101000010000_case_0,
//    pattern: cccc111011100001tttt101000010000,
//    rule: VMSR,
//    safety: [t  ==
//            Pc => UNPREDICTABLE],
//    t: Rt,
//    uses: {Rt}}
RegisterList VMSR_cccc111011100001tttt101000010000_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMSR_cccc111011100001tttt101000010000_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VMSR_cccc111011100001tttt101000010000_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// VMULL_by_scalar_A2_1111001u1dssnnnndddd1010n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMULL_by_scalar_A2_1111001u1dssnnnndddd1010n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001u1dssnnnndddd1010n1m0mmmm,
//    regs: 1,
//    rule: VMULL_by_scalar_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: Q(24)=1,
//    uses: {}}
RegisterList VMULL_by_scalar_A2_1111001u1dssnnnndddd1010n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMULL_by_scalar_A2_1111001u1dssnnnndddd1010n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMULL_by_scalar_A2_1111001u1dssnnnndddd1010n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMULL_integer_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_I8_16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMULL_integer_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd11p0n0m0mmmm,
//    rule: VMULL_integer_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMULL_integer_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMULL_integer_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(15:12)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMULL_integer_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_P8,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd11p0n0m0mmmm,
//    rule: VMULL_polynomial_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      U(24)=1 ||
//         size(21:20)=~00 => UNDEFINED,
//      Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(24)=1 ||
  //       inst(21:20)=~00 => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) ||
       ((inst.Bits() & 0x00300000)  !=
          0x00000000))
    return UNDEFINED;

  // inst(15:12)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_F32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_0,
//    index: M,
//    m: Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd100fn1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VMUL_by_scalar_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         size(21:20)=01) => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(21:20)=01) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00100000)))
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_1:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_1,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd100fn1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VMUL_by_scalar_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00000000)
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMUL_by_scalar_A1_1111001q1dssnnnndddd100fn1m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110d0snnnndddd1101nqm1mmmm,
//    rule: VMUL_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMUL_floating_point_A1_111100110d0snnnndddd1101nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1,
//    advsimd: false,
//    arch: VFPv2,
//    baseline: VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd
//         if dp_operation
//         else Vd:D,
//    defs: {},
//    dp_operation: sz(8)=1,
//    false: false,
//    fields: [cond(31:28),
//      D(22),
//      Vn(19:16),
//      Vd(15:12),
//      sz(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    pattern: cccc11100d10nnnndddd101sn0m0mmmm,
//    rule: VMUL_floating_point,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
RegisterList VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(31:28)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0xF0000000)  ==
          0xF0000000)
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VMUL_floating_point_cccc11100d10nnnndddd101sn0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMUL_integer_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMUL_integer_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8P,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    false: false,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMUL_polynomial_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=~00 => UNDEFINED],
//    size: size(21:20),
//    unsigned: false,
//    uses: {}}
RegisterList VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=~00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  !=
          0x00000000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0:
//
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    actual: Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_MVN,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
//    generated_baseline: VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
//    i: i(24),
//    imm3: imm3(18:16),
//    imm4: imm4(3:0),
//    imm64: AdvSIMDExpandImm(op, cmode, i:imm3:imm4),
//    op: op(5),
//    pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMVN_immediate,
//    safety: [(cmode(0)=1 &&
//         cmode(3:2)=~11) ||
//         cmode(3:1)=111 => DECODER_ERROR,
//      Q(6)=1 &&
//         Vd(0)=1 => UNDEFINED],
//    uses: {}}
RegisterList VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(11:8)(0)=1 &&
  //       inst(11:8)(3:2)=~11) ||
  //       inst(11:8)(3:1)=111 => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001)  ==
          0x00000001) &&
       ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C)  !=
          0x0000000C))) ||
       ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000E)  ==
          0x0000000E))
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd01011qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VMVN_register,
//    safety: [size(19:18)=~00 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=~00 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  !=
          0x00000000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VMVN_register_111100111d11ss00dddd01011qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f111qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VNEG,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_F32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VNEG_111100111d11ss01dddd0f111qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f111qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VNEG,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(19:18)=~10 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  !=
          0x00080000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VNEG_111100111d11ss01dddd0f111qm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VNEG_cccc11101d110001dddd101s01m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//    advsimd: false,
//    arch: VFPv2,
//    baseline: CondVfpOp,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D
//         if sz(8)=0
//         else D:Vd,
//    defs: {},
//    dp_operation: sz(8)=1,
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), sz(8), M(5), Vm(3:0)],
//    generated_baseline: VNEG_cccc11101d110001dddd101s01m0mmmm_case_0,
//    m: Vm:D
//         if sz(8)=0
//         else M:Vm,
//    pattern: cccc11101d110001dddd101s01m0mmmm,
//    rule: VNEG,
//    safety: [true => MAY_BE_SAFE],
//    sz: sz(8),
//    true: true,
//    uses: {}}
RegisterList VNEG_cccc11101d110001dddd101s01m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VNEG_cccc11101d110001dddd101s01m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => MAY_BE_SAFE
  if (true)
    return MAY_BE_SAFE;

  return MAY_BE_SAFE;
}


RegisterList VNEG_cccc11101d110001dddd101s01m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    VFPNegMul_VNMLA: 1,
//    VFPNegMul_VNMLS: 2,
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1,
//    arch: VFPv2,
//    baseline: VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd
//         if dp_operation
//         else Vd:D,
//    defs: {},
//    dp_operation: sz(8)=1,
//    fields: [cond(31:28),
//      D(22),
//      Vn(19:16),
//      Vd(15:12),
//      sz(8),
//      N(7),
//      op(6),
//      M(5),
//      Vm(3:0)],
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    op: op(6),
//    pattern: cccc11100d01nnnndddd101snom0mmmm,
//    rule: VNMLA_VNMLS,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    type: VFPNegMul_VNMLA
//         if op(6)=1
//         else VFPNegMul_VNMLS,
//    uses: {}}
RegisterList VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(31:28)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0xF0000000)  ==
          0xF0000000)
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VNMLA_VNMLS_cccc11100d01nnnndddd101snom0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    VFPNegMul_VNMUL: 3,
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1,
//    arch: VFPv2,
//    baseline: VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd
//         if dp_operation
//         else Vd:D,
//    defs: {},
//    dp_operation: sz(8)=1,
//    fields: [cond(31:28),
//      D(22),
//      Vn(19:16),
//      Vd(15:12),
//      sz(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    pattern: cccc11100d10nnnndddd101sn1m0mmmm,
//    rule: VNMUL,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    type: VFPNegMul_VNMUL,
//    uses: {}}
RegisterList VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(31:28)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0xF0000000)  ==
          0xF0000000)
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VNMUL_cccc11100d10nnnndddd101sn1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100d11nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VORN_register,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VORN_register_111100100d11nnnndddd0001nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0:
//
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    actual: Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_BIT,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
//    generated_baseline: VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0,
//    i: i(24),
//    imm3: imm3(18:16),
//    imm4: imm4(3:0),
//    imm64: AdvSIMDExpandImm(op, cmode, i:imm3:imm4),
//    op: op(5),
//    pattern: 1111001i1d000mmmddddcccc0q01mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VORR_immediate,
//    safety: [cmode(0)=0 ||
//         cmode(3:2)=11 => DECODER_ERROR,
//      Q(6)=1 &&
//         Vd(0)=1 => UNDEFINED],
//    uses: {}}
RegisterList VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:8)(0)=0 ||
  //       inst(11:8)(3:2)=11 => DECODER_ERROR
  if (((((inst.Bits() & 0x00000F00) >> 8) & 0x00000001)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x00000F00) >> 8) & 0x0000000C)  ==
          0x0000000C))
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VORR_immediate_1111001i1d000mmmddddcccc0q01mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100d10nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VORR_register_or_VMOV_register_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VORR_register_or_VMOV_register_A1_111100100d10nnnndddd0001nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd0110p1m0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VPADAL,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    unsigned: (op(0)=1),
//    uses: {}}
RegisterList VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPADAL_111100111d11ss00dddd0110p1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd0010p1m0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VPADDL,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    unsigned: (op(0)=1),
//    uses: {}}
RegisterList VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPADDL_111100111d11ss00dddd0010p1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32P,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110d0snnnndddd1101nqm0mmmm,
//    rule: VPADD_floating_point,
//    safety: [size(0)=1 ||
//         Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)(0)=1 ||
  //       inst(6)=1 => UNDEFINED
  if (((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001) ||
       ((inst.Bits() & 0x00000040)  ==
          0x00000040))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDI,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100dssnnnndddd1011n0m1mmmm,
//    rule: VPADD_integer,
//    safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  // inst(6)=1 => UNDEFINED
  if ((inst.Bits() & 0x00000040)  ==
          0x00000040)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32P,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110dssnnnndddd1111nqm0mmmm,
//    rule: VPMAX,
//    safety: [size(0)=1 ||
//         Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)(0)=1 ||
  //       inst(6)=1 => UNDEFINED
  if (((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001) ||
       ((inst.Bits() & 0x00000040)  ==
          0x00000040))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPMAX_111100110dssnnnndddd1111nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDI,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1010n0m0mmmm,
//    rule: VPMAX,
//    safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  // inst(6)=1 => UNDEFINED
  if ((inst.Bits() & 0x00000040)  ==
          0x00000040)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPMAX_1111001u0dssnnnndddd1010n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32P,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110dssnnnndddd1111nqm0mmmm,
//    rule: VPMIN,
//    safety: [size(0)=1 ||
//         Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)(0)=1 ||
  //       inst(6)=1 => UNDEFINED
  if (((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001) ||
       ((inst.Bits() & 0x00000040)  ==
          0x00000040))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPMIN_111100110dssnnnndddd1111nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDI,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1010n0m1mmmm,
//    rule: VPMIN,
//    safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  // inst(6)=1 => UNDEFINED
  if ((inst.Bits() & 0x00000040)  ==
          0x00000040)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VPMIN_1111001u0dssnnnndddd1010n0m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VPOP_cccc11001d111101dddd1010iiiiiiii_case_0:
//
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1,
//    arch: VFPv2,
//    base: Sp,
//    baseline: LoadVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {Sp},
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    generated_baseline: VPOP_cccc11001d111101dddd1010iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    pattern: cccc11001d111101dddd1010iiiiiiii,
//    regs: imm8,
//    rule: VPOP,
//    safety: [regs  ==
//            0 ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    single_regs: true,
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Sp}}
Register VPOP_cccc11001d111101dddd1010iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '13'
  return Register(13);
}

RegisterList VPOP_cccc11001d111101dddd1010iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{13}'
  return RegisterList().
   Add(Register(13));
}

SafetyLevel VPOP_cccc11001d111101dddd1010iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 0  ==
  //          inst(7:0) ||
  //       32  <=
  //          inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VPOP_cccc11001d111101dddd1010iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList VPOP_cccc11001d111101dddd1010iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{13}'
  return RegisterList().
   Add(Register(13));
}

// VPOP_cccc11001d111101dddd1011iiiiiiii_case_0:
//
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Sp,
//    baseline: LoadVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {Sp},
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    generated_baseline: VPOP_cccc11001d111101dddd1011iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    pattern: cccc11001d111101dddd1011iiiiiiii,
//    regs: imm8 / 2,
//    rule: VPOP,
//    safety: [regs  ==
//            0 ||
//         regs  >
//            16 ||
//         d + regs  >
//            32 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         d + regs  >
//            16 => UNPREDICTABLE,
//      imm8(0)  ==
//            1 => DEPRECATED],
//    single_regs: false,
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Sp}}
Register VPOP_cccc11001d111101dddd1011iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '13'
  return Register(13);
}

RegisterList VPOP_cccc11001d111101dddd1011iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{13}'
  return RegisterList().
   Add(Register(13));
}

SafetyLevel VPOP_cccc11001d111101dddd1011iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 0  ==
  //          inst(7:0) / 2 ||
  //       16  <=
  //          inst(7:0) / 2 ||
  //       32  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32))))
    return UNPREDICTABLE;

  // VFPSmallRegisterBank() &&
  //       16  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if ((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16))))
    return UNPREDICTABLE;

  // 1  ==
  //          inst(7:0)(0) => DEPRECATED
  if (((((inst.Bits() & 0x000000FF) & 0x00000001)) == (1)))
    return DEPRECATED;

  return MAY_BE_SAFE;
}


bool VPOP_cccc11001d111101dddd1011iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList VPOP_cccc11001d111101dddd1011iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{13}'
  return RegisterList().
   Add(Register(13));
}

// VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0:
//
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1,
//    arch: VFPv2,
//    base: Sp,
//    baseline: StoreVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {Sp},
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    generated_baseline: VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    pattern: cccc11010d101101dddd1010iiiiiiii,
//    regs: imm8,
//    rule: VPUSH,
//    safety: [regs  ==
//            0 ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    single_regs: true,
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Sp}}
Register VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '13'
  return Register(13);
}

RegisterList VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{13}'
  return RegisterList().
   Add(Register(13));
}

SafetyLevel VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 0  ==
  //          inst(7:0) ||
  //       32  <=
  //          inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList VPUSH_cccc11010d101101dddd1010iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{13}'
  return RegisterList().
   Add(Register(13));
}

// VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0:
//
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Sp,
//    baseline: StoreVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {Sp},
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    generated_baseline: VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    pattern: cccc11010d101101dddd1011iiiiiiii,
//    regs: imm8 / 2,
//    rule: VPUSH,
//    safety: [regs  ==
//            0 ||
//         regs  >
//            16 ||
//         d + regs  >
//            32 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         d + regs  >
//            16 => UNPREDICTABLE,
//      imm8(0)  ==
//            1 => DEPRECATED],
//    single_regs: false,
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Sp}}
Register VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '13'
  return Register(13);
}

RegisterList VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{13}'
  return RegisterList().
   Add(Register(13));
}

SafetyLevel VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 0  ==
  //          inst(7:0) / 2 ||
  //       16  <=
  //          inst(7:0) / 2 ||
  //       32  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32))))
    return UNPREDICTABLE;

  // VFPSmallRegisterBank() &&
  //       16  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if ((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16))))
    return UNPREDICTABLE;

  // 1  ==
  //          inst(7:0)(0) => DEPRECATED
  if (((((inst.Bits() & 0x000000FF) & 0x00000001)) == (1)))
    return DEPRECATED;

  return MAY_BE_SAFE;
}


bool VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList VPUSH_cccc11010d101101dddd1011iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{13}'
  return RegisterList().
   Add(Register(13));
}

// VQABS_111100111d11ss00dddd01110qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQABS_111100111d11ss00dddd01110qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd01110qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQABS,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VQABS_111100111d11ss00dddd01110qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQABS_111100111d11ss00dddd01110qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQABS_111100111d11ss00dddd01110qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0000nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQADD,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQADD_1111001u0dssnnnndddd0000nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMLAL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMLAL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 111100101dssnnnndddd0p11n1m0mmmm,
//    regs: 1,
//    rule: VQDMLAL_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: Q(24)=1,
//    uses: {}}
RegisterList VQDMLAL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMLAL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMLAL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1,
//    add: op(8)=0,
//    baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_0,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 111100101dssnnnndddd10p1n0m0mmmm,
//    rule: VQDMLAL_VQDMLSL_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 ||
//         Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 ||
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMLSL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMLSL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 111100101dssnnnndddd0p11n1m0mmmm,
//    regs: 1,
//    rule: VQDMLSL_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: Q(24)=1,
//    uses: {}}
RegisterList VQDMLSL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMLSL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMLSL_A1_111100101dssnnnndddd0p11n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100dssnnnndddd1011nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQDMULH_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      (size(21:20)=11 ||
//         size(21:20)=00) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // (inst(21:20)=11 ||
  //       inst(21:20)=00) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00300000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00000000)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMULH_A2_1111001q1dssnnnndddd1100n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMULH_A2_1111001q1dssnnnndddd1100n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd1100n1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VQDMULH_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VQDMULH_A2_1111001q1dssnnnndddd1100n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMULH_A2_1111001q1dssnnnndddd1100n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00000000)
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMULH_A2_1111001q1dssnnnndddd1100n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMULL_A1_111100101dssnnnndddd1101n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1,
//    add: op(8)=0,
//    baseline: VectorBinary3RegisterDifferentLength_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMULL_A1_111100101dssnnnndddd1101n0m0mmmm_case_0,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 111100101dssnnnndddd1101n0m0mmmm,
//    rule: VQDMULL_A1,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 ||
//         Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQDMULL_A1_111100101dssnnnndddd1101n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMULL_A1_111100101dssnnnndddd1101n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 ||
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMULL_A1_111100101dssnnnndddd1101n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQDMULL_A2_111100101dssnnnndddd1011n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32L,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQDMULL_A2_111100101dssnnnndddd1011n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 111100101dssnnnndddd1011n1m0mmmm,
//    regs: 1,
//    rule: VQDMULL_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      (size(21:20)=00 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: Q(24)=1,
//    uses: {}}
RegisterList VQDMULL_A2_111100101dssnnnndddd1011n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQDMULL_A2_111100101dssnnnndddd1011n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // (inst(21:20)=00 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00000000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQDMULL_A2_111100101dssnnnndddd1011n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: op(0)=1,
//    fields: [D(22), size(19:18), Vd(15:12), op(7:6), M(5), Vm(3:0)],
//    generated_baseline: VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//    m: M:Vm,
//    op: op(7:6),
//    pattern: 111100111d11ss10dddd0010ppm0mmmm,
//    rule: VQMOVN,
//    safety: [op(7:6)=00 => DECODER_ERROR,
//      size(19:18)=11 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(19:18),
//    src_unsigned: op(7:6)=11,
//    uses: {}}
RegisterList VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=00 => DECODER_ERROR
  if ((inst.Bits() & 0x000000C0)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(19:18)=11 ||
  //       inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: op(0)=1,
//    fields: [D(22), size(19:18), Vd(15:12), op(7:6), M(5), Vm(3:0)],
//    generated_baseline: VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0,
//    m: M:Vm,
//    op: op(7:6),
//    pattern: 111100111d11ss10dddd0010ppm0mmmm,
//    rule: VQMOVUN,
//    safety: [op(7:6)=00 => DECODER_ERROR,
//      size(19:18)=11 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(19:18),
//    src_unsigned: op(7:6)=11,
//    uses: {}}
RegisterList VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=00 => DECODER_ERROR
  if ((inst.Bits() & 0x000000C0)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(19:18)=11 ||
  //       inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQMOVUN_111100111d11ss10dddd0010ppm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd01111qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQNEG,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQNEG_111100111d11ss00dddd01111qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQRDMULH_1111001q1dssnnnndddd1101n1m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterScalar_I16_32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [Q(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(10),
//      F(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQRDMULH_1111001q1dssnnnndddd1101n1m0mmmm_case_0,
//    index: M:Vm(3)
//         if size(21:20)=01
//         else M,
//    m: Vm(2:0)
//         if size(21:20)=01
//         else Vm,
//    n: N:Vn,
//    op: op(10),
//    pattern: 1111001q1dssnnnndddd1101n1m0mmmm,
//    regs: 1
//         if Q(24)=0
//         else 2,
//    rule: VQRDMULH,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      size(21:20)=00 => UNDEFINED,
//      Q(24)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
RegisterList VQRDMULH_1111001q1dssnnnndddd1101n1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQRDMULH_1111001q1dssnnnndddd1101n1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(21:20)=00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00000000)
    return UNDEFINED;

  // inst(24)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQRDMULH_1111001q1dssnnnndddd1101n1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110dssnnnndddd1011nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQRDMULH_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      (size(21:20)=11 ||
//         size(21:20)=00) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // (inst(21:20)=11 ||
  //       inst(21:20)=00) => UNDEFINED
  if ((((inst.Bits() & 0x00300000)  ==
          0x00300000) ||
       ((inst.Bits() & 0x00300000)  ==
          0x00000000)))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQRDMULH_A1_111100110dssnnnndddd1011nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0101nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQRSHL,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQRSHL_1111001u0dssnnnndddd0101nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: U(24)=1,
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQRSHRN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//      Vm(0)=1 => UNDEFINED,
//      U(24)=0 &&
//         op(8)=0 => DECODER_ERROR],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    src_unsigned: U(24)=1 &&
//         op(8)=1,
//    uses: {}}
RegisterList VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  // inst(24)=0 &&
  //       inst(8)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000100)  ==
          0x00000000))
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: U(24)=1,
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQRSHRUN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//      Vm(0)=1 => UNDEFINED,
//      U(24)=0 &&
//         op(8)=0 => DECODER_ERROR],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    src_unsigned: U(24)=1 &&
//         op(8)=1,
//    uses: {}}
RegisterList VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  // inst(24)=0 &&
  //       inst(8)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000100)  ==
          0x00000000))
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VQRSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: U(24)=1,
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd100p01m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQRSHRUN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//      Vm(0)=1 => UNDEFINED,
//      U(24)=0 &&
//         op(8)=0 => DECODER_ERROR],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    src_unsigned: U(24)=1 &&
//         op(8)=1,
//    uses: {}}
RegisterList VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  // inst(24)=0 &&
  //       inst(8)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000100)  ==
          0x00000000))
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VQRSHRUN_1111001u1diiiiiidddd100p01m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_ILS,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: U(24)=1,
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd011plqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQSHL_VQSHLU_immediate,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      U(24)=0 &&
//         op(8)=0 => UNDEFINED],
//    shift_amount: imm6 - 8
//         if L:imm6(6:0)=0001xxx
//         else imm6 - 16
//         if L:imm6(6:0)=001xxxx
//         else imm6 - 32
//         if L:imm6(6:0)=01xxxxx
//         else imm6
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    src_unsigned: U(24)=1 &&
//         op(8)=1,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(24)=0 &&
  //       inst(8)=0 => UNDEFINED
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000100)  ==
          0x00000000))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0100nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQSHL_register,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQSHL_register_1111001u0dssnnnndddd0100nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: U(24)=1,
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQSHRN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//      Vm(0)=1 => UNDEFINED,
//      U(24)=0 &&
//         op(8)=0 => DECODER_ERROR],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    src_unsigned: U(24)=1 &&
//         op(8)=1,
//    uses: {}}
RegisterList VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  // inst(24)=0 &&
  //       inst(8)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000100)  ==
          0x00000000))
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VQSHRN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64RS,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    dest_unsigned: U(24)=1,
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd100p00m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQSHRUN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//      Vm(0)=1 => UNDEFINED,
//      U(24)=0 &&
//         op(8)=0 => DECODER_ERROR],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    src_unsigned: U(24)=1 &&
//         op(8)=1,
//    uses: {}}
RegisterList VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  // inst(24)=0 &&
  //       inst(8)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00000100)  ==
          0x00000000))
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VQSHRUN_1111001u1diiiiiidddd100p00m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0010nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VQSUB,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VQSUB_1111001u0dssnnnndddd0010nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRADDHN_111100111dssnnnndddd0100n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRADDHN_111100111dssnnnndddd0100n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 111100111dssnnnndddd0100n0m0mmmm,
//    rule: VRADDHN,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      Vn(0)=1 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VRADDHN_111100111dssnnnndddd0100n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRADDHN_111100111dssnnnndddd0100n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1 => UNDEFINED
  if (((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRADDHN_111100111dssnnnndddd0100n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_F32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    floating_point: F(10)=1,
//    generated_baseline: VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss11dddd010f0qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRECPE,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(19:18)=~10 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  !=
          0x00080000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRECPE_111100111d11ss11dddd010f0qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d0snnnndddd1111nqm1mmmm,
//    rule: VRECPS,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRECPS_111100100d0snnnndddd1111nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_RG,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0,
//    groupsize: rev_groupsize(op, size),
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd000ppqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    reverse_mask: rev_mask(groupsize, esize),
//    rule: VREV16,
//    safety: [op + size  >=
//            3 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 3  <
  //          inst(8:7) + inst(19:18) => UNDEFINED
  if (((((inst.Bits() & 0x00000180) >> 7) + ((inst.Bits() & 0x000C0000) >> 18)) >= (3)))
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VREV16_111100111d11ss00dddd000ppqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_RG,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0,
//    groupsize: rev_groupsize(op, size),
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd000ppqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    reverse_mask: rev_mask(groupsize, esize),
//    rule: VREV32,
//    safety: [op + size  >=
//            3 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 3  <
  //          inst(8:7) + inst(19:18) => UNDEFINED
  if (((((inst.Bits() & 0x00000180) >> 7) + ((inst.Bits() & 0x000C0000) >> 18)) >= (3)))
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VREV32_111100111d11ss00dddd000ppqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_RG,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0,
//    groupsize: rev_groupsize(op, size),
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd000ppqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    reverse_mask: rev_mask(groupsize, esize),
//    rule: VREV64,
//    safety: [op + size  >=
//            3 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 3  <
  //          inst(8:7) + inst(19:18) => UNDEFINED
  if (((((inst.Bits() & 0x00000180) >> 7) + ((inst.Bits() & 0x000C0000) >> 18)) >= (3)))
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VREV64_111100111d11ss00dddd000ppqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0001nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRHADD,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRHADD_1111001u0dssnnnndddd0001nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0101nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRSHL,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSHL_1111001u0dssnnnndddd0101nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 111100101diiiiiidddd100001m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRSHRN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    uses: {}}
RegisterList VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSHRN_111100101diiiiiidddd100001m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd0010lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRSHR,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: 16 - imm6
//         if L:imm6(6:0)=0001xxx
//         else 32 - imm6
//         if L:imm6(6:0)=001xxxx
//         else 64 - imm6,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_F32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    floating_point: F(10)=1,
//    generated_baseline: VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss11dddd010f1qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRSQRTE,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(19:18)=~10 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  !=
          0x00080000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSQRTE_111100111d11ss11dddd010f1qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d1snnnndddd1111nqm1mmmm,
//    rule: VRSQRTS,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSQRTS_111100100d1snnnndddd1111nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd0011lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VRSRA,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: 16 - imm6
//         if L:imm6(6:0)=0001xxx
//         else 32 - imm6
//         if L:imm6(6:0)=001xxxx
//         else 64 - imm6,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSRA_1111001u1diiiiiidddd0011lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VRSUBHN_111100111dssnnnndddd0110n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VRSUBHN_111100111dssnnnndddd0110n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 111100111dssnnnndddd0110n0m0mmmm,
//    rule: VRSUBHN,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      Vn(0)=1 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VRSUBHN_111100111dssnnnndddd0110n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VRSUBHN_111100111dssnnnndddd0110n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1 => UNDEFINED
  if (((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VRSUBHN_111100111dssnnnndddd0110n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_E8_16_32L,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd101000m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSHLL_A1_or_VMOVL,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    shift_amount: imm6 - 8
//         if imm6(21:16)=001xxx
//         else imm6 - 16
//         if imm6(21:16)=01xxxx
//         else imm6 - 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    uses: {}}
RegisterList VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(15:12)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_I8_16_32L,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss10dddd001100m0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSHLL_A2,
//    safety: [size(19:18)=11 ||
//         Vd(0)=1 => UNDEFINED],
//    shift_amount: esize,
//    size: size(19:18),
//    uses: {}}
RegisterList VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=11 ||
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 111100101diiiiiidddd0101lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSHL_immediate,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: imm6 - 8
//         if L:imm6(6:0)=0001xxx
//         else imm6 - 16
//         if L:imm6(6:0)=001xxxx
//         else imm6 - 32
//         if L:imm6(6:0)=01xxxxx
//         else imm6
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSHL_immediate_111100101diiiiiidddd0101lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0100nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSHL_register,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSHL_register_1111001u0dssnnnndddd0100nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSHRN_111100101diiiiiidddd100000m1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_N16_32_64R,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if imm6(21:16)=001xxx
//         else 4
//         if imm6(21:16)=01xxxx
//         else 2
//         if imm6(21:16)=1xxxxx
//         else 0,
//    esize: 8
//         if imm6(21:16)=001xxx
//         else 16
//         if imm6(21:16)=01xxxx
//         else 32
//         if imm6(21:16)=1xxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSHRN_111100101diiiiiidddd100000m1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 111100101diiiiiidddd100000m1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSHRN,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR, Vm(0)=1 => UNDEFINED],
//    shift_amount: 16 - imm6
//         if imm6(21:16)=001xxx
//         else 32 - imm6
//         if imm6(21:16)=01xxxx
//         else 64 - imm6
//         if imm6(21:16)=1xxxxx
//         else 0,
//    uses: {}}
RegisterList VSHRN_111100101diiiiiidddd100000m1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSHRN_111100101diiiiiidddd100000m1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(3:0)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSHRN_111100101diiiiiidddd100000m1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd0000lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSHR,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: 16 - imm6
//         if L:imm6(6:0)=0001xxx
//         else 32 - imm6
//         if L:imm6(6:0)=001xxxx
//         else 64 - imm6,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSHR_1111001u1diiiiiidddd0000lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 111100111diiiiiidddd0101lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSLI,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: imm6 - 8
//         if L:imm6(6:0)=0001xxx
//         else imm6 - 16
//         if L:imm6(6:0)=001xxxx
//         else imm6 - 32
//         if L:imm6(6:0)=01xxxxx
//         else imm6
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSLI_111100111diiiiiidddd0101lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VABS_cccc11101d110000dddd101s11m0mmmm_case_1,
//    advsimd: false,
//    arch: VFPv2,
//    baseline: CondVfpOp,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D
//         if sz(8)=0
//         else D:Vd,
//    defs: {},
//    dp_operation: sz(8)=1,
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), sz(8), M(5), Vm(3:0)],
//    generated_baseline: VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0,
//    m: Vm:D
//         if sz(8)=0
//         else M:Vm,
//    pattern: cccc11101d110001dddd101s11m0mmmm,
//    rule: VSQRT,
//    safety: [true => MAY_BE_SAFE],
//    sz: sz(8),
//    true: true,
//    uses: {}}
RegisterList VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => MAY_BE_SAFE
  if (true)
    return MAY_BE_SAFE;

  return MAY_BE_SAFE;
}


RegisterList VSQRT_cccc11101d110001dddd101s11m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd0001lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSRA,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: 16 - imm6
//         if L:imm6(6:0)=0001xxx
//         else 32 - imm6
//         if L:imm6(6:0)=001xxxx
//         else 64 - imm6,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSRA_1111001u1diiiiiidddd0001lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0:
//
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 8
//         if L:imm6(6:0)=0001xxx
//         else 4
//         if L:imm6(6:0)=001xxxx
//         else 2
//         if L:imm6(6:0)=01xxxxx
//         else 1
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    esize: 8
//         if L:imm6(6:0)=0001xxx
//         else 16
//         if L:imm6(6:0)=001xxxx
//         else 32
//         if L:imm6(6:0)=01xxxxx
//         else 64
//         if L:imm6(6:0)=1xxxxxx
//         else 0,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 111100111diiiiiidddd0100lqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSRI,
//    safety: [L:imm6(6:0)=0000xxx => DECODER_ERROR,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    shift_amount: 16 - imm6
//         if L:imm6(6:0)=0001xxx
//         else 32 - imm6
//         if L:imm6(6:0)=001xxxx
//         else 64 - imm6,
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR
  if (((((((inst.Bits() & 0x00000080) >> 7)) << 6) | ((inst.Bits() & 0x003F0000) >> 16)) & 0x00000078)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSRI_111100111diiiiiidddd0100lqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(5:4)=00
//         else 4 << align,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple1,
//    constraints: ,
//    d: D:Vd,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d00nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    regs: 1
//         if type(11:8)=0111
//         else 2
//         if type(11:8)=1010
//         else 3
//         if type(11:8)=0110
//         else 4
//         if type(11:8)=0010
//         else 0,
//    rule: VST1_multiple_single_elements,
//    safety: [type(11:8)=0111 &&
//         align(1)=1 => UNDEFINED,
//      type(11:8)=1010 &&
//         align(5:4)=11 => UNDEFINED,
//      type(11:8)=0110 &&
//         align(1)=1 => UNDEFINED,
//      not type in bitset {'0111', '1010', '0110', '0010'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:8)=0111 &&
  //       inst(5:4)(1)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000F00)  ==
          0x00000700) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002))
    return UNDEFINED;

  // inst(11:8)=1010 &&
  //       inst(5:4)=11 => UNDEFINED
  if (((inst.Bits() & 0x00000F00)  ==
          0x00000A00) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030))
    return UNDEFINED;

  // inst(11:8)=0110 &&
  //       inst(5:4)(1)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000F00)  ==
          0x00000600) &&
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002))
    return UNDEFINED;

  // not inst(11:8)=0111 ||
  //       inst(11:8)=1010 ||
  //       inst(11:8)=0110 ||
  //       inst(11:8)=0010 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000700) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000A00) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000600) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000200)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       32  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=0111
  //       else 2
  //       if inst(11:8)=1010
  //       else 3
  //       if inst(11:8)=0110
  //       else 4
  //       if inst(11:8)=0010
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000700
       ? 1
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000A00
       ? 2
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000600
       ? 3
       : ((inst.Bits() & 0x00000F00)  ==
          0x00000200
       ? 4
       : 0))))) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST1_multiple_single_elements_111101000d00nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1,
//    alignment: 1
//         if size(11:10)=00
//         else (1
//         if index_align(0)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(1:0)=00
//         else 4)
//         if size(11:10)=10
//         else 0,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle1,
//    constraints: ,
//    d: D:Vd,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d00nnnnddddss00aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VST1_single_element_from_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=00 &&
//         index_align(0)=~0 => UNDEFINED,
//      size(11:10)=01 &&
//         index_align(1)=~0 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(2)=~0 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1:0)=~00 &&
//         index_align(1:0)=~11 => UNDEFINED,
//      n  ==
//            Pc => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=00 &&
  //       inst(7:4)(0)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=01 &&
  //       inst(7:4)(1)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(2)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1:0)=~00 &&
  //       inst(7:4)(1:0)=~11 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000003))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST1_single_element_from_one_lane_111101001d00nnnnddddss00aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(5:4)=00
//         else 4 << align,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple2,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//    inc: 1
//         if type(11:8)=1000
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d00nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    regs: 1
//         if type in bitset {'1000', '1001'}
//         else 2,
//    rule: VST2_multiple_2_element_structures,
//    safety: [size(7:6)=11 => UNDEFINED,
//      type in bitset {'1000', '1001'} &&
//         align(5:4)=11 => UNDEFINED,
//      not type in bitset {'1000', '1001', '0011'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d2 + regs  >
//            32 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 => UNDEFINED
  if ((inst.Bits() & 0x000000C0)  ==
          0x000000C0)
    return UNDEFINED;

  // inst(11:8)=1000 ||
  //       inst(11:8)=1001 &&
  //       inst(5:4)=11 => UNDEFINED
  if ((((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)) &&
       ((inst.Bits() & 0x00000030)  ==
          0x00000030))
    return UNDEFINED;

  // not inst(11:8)=1000 ||
  //       inst(11:8)=1001 ||
  //       inst(11:8)=0011 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000300)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       32  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=1000
  //       else 2 + 1
  //       if inst(11:8)=1000 ||
  //       inst(11:8)=1001
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000800
       ? 1
       : 2) + (((inst.Bits() & 0x00000F00)  ==
          0x00000800) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000900)
       ? 1
       : 2)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST2_multiple_2_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1,
//    alignment: (1
//         if index_align(0)=0
//         else 2)
//         if size(11:10)=00
//         else (1
//         if index_align(0)=0
//         else 4)
//         if size(11:10)=01
//         else (1
//         if index_align(0)=0
//         else 8)
//         if size(11:10)=10
//         else 0,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle2,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d00nnnnddddss01aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VST2_single_2_element_structure_from_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1)=~0 => UNDEFINED,
//      n  ==
//            Pc ||
//         d2  >
//            31 => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  !=
          0x00000000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST2_single_2_element_structure_from_one_lane_111101001d00nnnnddddss01aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(0)=0
//         else 8,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple3,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//    inc: 1
//         if type(11:8)=0100
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d00nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VST3_multiple_3_element_structures,
//    safety: [size(7:6)=11 ||
//         align(1)=1 => UNDEFINED,
//      not type in bitset {'0100', '0101'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d3  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 ||
  //       inst(5:4)(1)=1 => UNDEFINED
  if (((inst.Bits() & 0x000000C0)  ==
          0x000000C0) ||
       ((((inst.Bits() & 0x00000030) >> 4) & 0x00000002)  ==
          0x00000002))
    return UNDEFINED;

  // not inst(11:8)=0100 ||
  //       inst(11:8)=0101 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000400) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000500)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=0100
  //       else 2 + 1
  //       if inst(11:8)=0100
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000400
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST3_multiple_3_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1,
//    alignment: 1,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle3,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d00nnnnddddss10aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VST3_single_3_element_structure_from_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=00 &&
//         index_align(0)=~0 => UNDEFINED,
//      size(11:10)=01 &&
//         index_align(0)=~0 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1:0)=~00 => UNDEFINED,
//      n  ==
//            Pc ||
//         d3  >
//            31 => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=00 &&
  //       inst(7:4)(0)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000000) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=01 &&
  //       inst(7:4)(0)=~0 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000400) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000001)  !=
          0x00000000))
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1:0)=~00 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  !=
          0x00000000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST3_single_3_element_structure_from_one_lane_111101001d00nnnnddddss10aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1,
//    align: align(5:4),
//    alignment: 1
//         if align(5:4)=00
//         else 4 << align,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreMultiple4,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    d4: d3 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    elements: 8 / ebytes,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      type(11:8),
//      size(7:6),
//      align(5:4),
//      Rm(3:0)],
//    generated_baseline: VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0,
//    inc: 1
//         if type(11:8)=0000
//         else 2,
//    m: Rm,
//    n: Rn,
//    pattern: 111101000d00nnnnddddttttssaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VST4_multiple_4_element_structures,
//    safety: [size(7:6)=11 => UNDEFINED,
//      not type in bitset {'0000', '0001'} => DECODER_ERROR,
//      n  ==
//            Pc ||
//         d4  >
//            31 => UNPREDICTABLE],
//    size: size(7:6),
//    small_imm_base_wb: wback &&
//         not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(7:6)=11 => UNDEFINED
  if ((inst.Bits() & 0x000000C0)  ==
          0x000000C0)
    return UNDEFINED;

  // not inst(11:8)=0000 ||
  //       inst(11:8)=0001 => DECODER_ERROR
  if (!(((inst.Bits() & 0x00000F00)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00000F00)  ==
          0x00000100)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:8)=0000
  //       else 2 + 1
  //       if inst(11:8)=0000
  //       else 2 + 1
  //       if inst(11:8)=0000
  //       else 2 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2) + ((inst.Bits() & 0x00000F00)  ==
          0x00000000
       ? 1
       : 2)) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST4_multiple_4_element_structures_111101000d00nnnnddddttttssaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0:
//
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
//    actual: Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1,
//    alignment: (1
//         if index_align(0)=0
//         else 4)
//         if size(11:10)=00
//         else (1
//         if index_align(0)=0
//         else 8)
//         if size(11:10)=01
//         else (1
//         if index_align(1:0)=00
//         else 4 << index_align(1:0))
//         if size(11:10)=10
//         else 0,
//    arch: ASIMD,
//    base: n,
//    baseline: VectorLoadStoreSingle4,
//    constraints: ,
//    d: D:Vd,
//    d2: d + inc,
//    d3: d2 + inc,
//    d4: d3 + inc,
//    defs: {base}
//         if wback
//         else {},
//    ebytes: 1 << size,
//    esize: 8 * ebytes,
//    fields: [D(22),
//      Rn(19:16),
//      Vd(15:12),
//      size(11:10),
//      index_align(7:4),
//      Rm(3:0)],
//    generated_baseline: VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0,
//    inc: 1
//         if size(11:10)=00
//         else (1
//         if index_align(1)=0
//         else 2)
//         if size(11:10)=01
//         else (1
//         if index_align(2)=0
//         else 2)
//         if size(11:10)=10
//         else 0,
//    index: index_align(3:1)
//         if size(11:10)=00
//         else index_align(3:2)
//         if size(11:10)=01
//         else index_align(3)
//         if size(11:10)=10
//         else 0,
//    index_align: index_align(7:4),
//    m: Rm,
//    n: Rn,
//    pattern: 111101001d00nnnnddddss11aaaammmm,
//    register_index: (m  !=
//            Pc &&
//         m  !=
//            Sp),
//    rule: VST4_single_4_element_structure_form_one_lane,
//    safety: [size(11:10)=11 => UNDEFINED,
//      size(11:10)=10 &&
//         index_align(1:0)=11 => UNDEFINED,
//      n  ==
//            Pc ||
//         d4  >
//            31 => UNPREDICTABLE],
//    size: size(11:10),
//    small_imm_base_wb: wback &&
//         not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
Register VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}
  //       if (15  !=
  //          inst(3:0))
  //       else {}'
  return (((((inst.Bits() & 0x0000000F)) != (15)))
       ? RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)))
       : RegisterList());
}

SafetyLevel VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(11:10)=11 => UNDEFINED
  if ((inst.Bits() & 0x00000C00)  ==
          0x00000C00)
    return UNDEFINED;

  // inst(11:10)=10 &&
  //       inst(7:4)(1:0)=11 => UNDEFINED
  if (((inst.Bits() & 0x00000C00)  ==
          0x00000800) &&
       ((((inst.Bits() & 0x000000F0) >> 4) & 0x00000003)  ==
          0x00000003))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) ||
  //       31  <=
  //          inst(22):inst(15:12) + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 + 1
  //       if inst(11:10)=00
  //       else (1
  //       if inst(7:4)(1)=0
  //       else 2)
  //       if inst(11:10)=01
  //       else (1
  //       if inst(7:4)(2)=0
  //       else 2)
  //       if inst(11:10)=10
  //       else 0 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0))) + ((inst.Bits() & 0x00000C00)  ==
          0x00000000
       ? 1
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000400
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000002)  ==
          0x00000000
       ? 1
       : 2))
       : ((inst.Bits() & 0x00000C00)  ==
          0x00000800
       ? (((((inst.Bits() & 0x000000F0) >> 4) & 0x00000004)  ==
          0x00000000
       ? 1
       : 2))
       : 0)))) > (31))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(15  !=
  //          inst(3:0)) &&
  //       not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return (((((inst.Bits() & 0x0000000F)) != (15)))) &&
       (!((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13))))));
}

RegisterList VST4_single_4_element_structure_form_one_lane_111101001d00nnnnddddss11aaaammmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)
  //       if (15  !=
  //          inst(3:0))
  //       else 32, inst(19:16)}'
  return RegisterList().
   Add(Register((((((inst.Bits() & 0x0000000F)) != (15)))
       ? (inst.Bits() & 0x0000000F)
       : 32))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0:
//
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
//    actual: Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: VFPv2,
//    base: Rn,
//    baseline: StoreVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {Rn
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      D(22),
//      W(21),
//      Rn(19:16),
//      Vd(15:12),
//      imm8(7:0)],
//    generated_baseline: VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    n: Rn,
//    pattern: cccc110pudw0nnnndddd1010iiiiiiii,
//    regs: imm8,
//    rule: VSTM,
//    safety: [P(24)=0 &&
//         U(23)=0 &&
//         W(21)=0 => DECODER_ERROR,
//      P(24)=1 &&
//         W(21)=0 => DECODER_ERROR,
//      P  ==
//            U &&
//         W(21)=1 => UNDEFINED,
//      n  ==
//            Pc &&
//         wback => UNPREDICTABLE,
//      P(24)=1 &&
//         U(23)=0 &&
//         W(21)=1 &&
//         Rn  ==
//            Sp => DECODER_ERROR,
//      Rn  ==
//            Pc => FORBIDDEN_OPERANDS,
//      regs  ==
//            0 ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    single_regs: true,
//    small_imm_base_wb: wback,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
Register VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(23)=0 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(24)=1 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(23)  ==
  //          inst(24) &&
  //       inst(21)=1 => UNDEFINED
  if ((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) &&
  //       inst(21)=1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // inst(24)=1 &&
  //       inst(23)=0 &&
  //       inst(21)=1 &&
  //       13  ==
  //          inst(19:16) => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13))))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return FORBIDDEN_OPERANDS;

  // 0  ==
  //          inst(7:0) ||
  //       32  <=
  //          inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0:
//
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
//    actual: Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Rn,
//    baseline: StoreVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {Rn
//         if wback
//         else None},
//    false: false,
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      D(22),
//      W(21),
//      Rn(19:16),
//      Vd(15:12),
//      imm8(7:0)],
//    generated_baseline: VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    n: Rn,
//    pattern: cccc110pudw0nnnndddd1011iiiiiiii,
//    regs: imm8 / 2,
//    rule: VSTM,
//    safety: [P(24)=0 &&
//         U(23)=0 &&
//         W(21)=0 => DECODER_ERROR,
//      P(24)=1 &&
//         W(21)=0 => DECODER_ERROR,
//      P  ==
//            U &&
//         W(21)=1 => UNDEFINED,
//      n  ==
//            Pc &&
//         wback => UNPREDICTABLE,
//      P(24)=1 &&
//         U(23)=0 &&
//         W(21)=1 &&
//         Rn  ==
//            Sp => DECODER_ERROR,
//      Rn  ==
//            Pc => FORBIDDEN_OPERANDS,
//      regs  ==
//            0 ||
//         regs  >
//            16 ||
//         d + regs  >
//            32 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         d + regs  >
//            16 => UNPREDICTABLE,
//      imm8(0)  ==
//            1 => DEPRECATED],
//    single_regs: false,
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: W(21)=1}
Register VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(23)=0 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(24)=1 &&
  //       inst(21)=0 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00000000))
    return DECODER_ERROR;

  // inst(23)  ==
  //          inst(24) &&
  //       inst(21)=1 => UNDEFINED
  if ((((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00800000) >> 23)))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNDEFINED;

  // 15  ==
  //          inst(19:16) &&
  //       inst(21)=1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // inst(24)=1 &&
  //       inst(23)=0 &&
  //       inst(21)=1 &&
  //       13  ==
  //          inst(19:16) => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (13))))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return FORBIDDEN_OPERANDS;

  // 0  ==
  //          inst(7:0) / 2 ||
  //       16  <=
  //          inst(7:0) / 2 ||
  //       32  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF) / 2) == (0))) ||
       ((((inst.Bits() & 0x000000FF) / 2) > (16))) ||
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (32))))
    return UNPREDICTABLE;

  // VFPSmallRegisterBank() &&
  //       16  <=
  //          inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE
  if ((nacl_arm_dec::VFPSmallRegisterBank()) &&
       ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12)) + (inst.Bits() & 0x000000FF) / 2) > (16))))
    return UNPREDICTABLE;

  // 1  ==
  //          inst(7:0)(0) => DEPRECATED
  if (((((inst.Bits() & 0x000000FF) & 0x00000001)) == (1)))
    return DEPRECATED;

  return MAY_BE_SAFE;
}


bool VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_0:
//
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    actual: Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: VFPv2,
//    base: Rn,
//    baseline: StoreVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {},
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    generated_baseline: VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    n: Rn,
//    pattern: cccc1101ud00nnnndddd1010iiiiiiii,
//    rule: VSTR,
//    safety: [n  ==
//            Pc => FORBIDDEN_OPERANDS],
//    single_reg: true,
//    true: true,
//    uses: {Rn}}
Register VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VSTR_cccc1101ud00nnnndddd1011iiiiiiii_case_0:
//
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    actual: Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1,
//    add: U(23)=1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Rn,
//    baseline: StoreVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    false: false,
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    generated_baseline: VSTR_cccc1101ud00nnnndddd1011iiiiiiii_case_0,
//    imm32: ZeroExtend(imm8:'00'(1:0), 32),
//    imm8: imm8(7:0),
//    n: Rn,
//    pattern: cccc1101ud00nnnndddd1011iiiiiiii,
//    rule: VSTR,
//    safety: [n  ==
//            Pc => FORBIDDEN_OPERANDS],
//    single_reg: false,
//    uses: {Rn}}
Register VSTR_cccc1101ud00nnnndddd1011iiiiiiii_case_0::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList VSTR_cccc1101ud00nnnndddd1011iiiiiiii_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSTR_cccc1101ud00nnnndddd1011iiiiiiii_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList VSTR_cccc1101ud00nnnndddd1011iiiiiiii_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// VSUBHN_111100101dssnnnndddd0110n0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_I16_32_64,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSUBHN_111100101dssnnnndddd0110n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 111100101dssnnnndddd0110n0m0mmmm,
//    rule: VSUBHN,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      Vn(0)=1 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSUBHN_111100101dssnnnndddd0110n0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSUBHN_111100101dssnnnndddd0110n0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1 => UNDEFINED
  if (((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSUBHN_111100101dssnnnndddd0110n0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSUBL_VSUBW_1111001u1dssnnnndddd001pn0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1,
//    baseline: VectorBinary3RegisterDifferentLength_I8_16_32,
//    constraints: & size(21:20)=~11 ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSUBL_VSUBW_1111001u1dssnnnndddd001pn0m0mmmm_case_0,
//    is_w: op(8)=1,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd001pn0m0mmmm,
//    rule: VSUBL_VSUBW,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      Vd(0)=1 ||
//         (op(8)=1 &&
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSUBL_VSUBW_1111001u1dssnnnndddd001pn0m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSUBL_VSUBW_1111001u1dssnnnndddd001pn0m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:20)=11 => DECODER_ERROR
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return DECODER_ERROR;

  // inst(15:12)(0)=1 ||
  //       (inst(8)=1 &&
  //       inst(19:16)(0)=1) => UNDEFINED
  if (((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x00000100)  ==
          0x00000100) &&
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSUBL_VSUBW_1111001u1dssnnnndddd001pn0m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLength32_DQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d1snnnndddd1101nqm0mmmm,
//    rule: VSUB_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
RegisterList VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSUB_floating_point_A1_111100100d1snnnndddd1101nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_1,
//    advsimd: false,
//    arch: VFPv2,
//    baseline: VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd
//         if dp_operation
//         else Vd:D,
//    defs: {},
//    dp_operation: sz(8)=1,
//    false: false,
//    fields: [cond(31:28),
//      D(22),
//      Vn(19:16),
//      Vd(15:12),
//      sz(8),
//      N(7),
//      M(5),
//      Vm(3:0)],
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    pattern: cccc11100d11nnnndddd101sn1m0mmmm,
//    rule: VSUB_floating_point,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
RegisterList VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(31:28)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0xF0000000)  ==
          0xF0000000)
    return DECODER_ERROR;

  return MAY_BE_SAFE;
}


RegisterList VSUB_floating_point_cccc11100d11nnnndddd101sn1m0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQ,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110dssnnnndddd1000nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSUB_integer,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSUB_integer_111100110dssnnnndddd1000nqm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VSWP_111100111d11ss10dddd00000qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8S,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VSWP_111100111d11ss10dddd00000qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss10dddd00000qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VSWP,
//    safety: [d  ==
//            m => UNKNOWN,
//      size(19:18)=~00 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VSWP_111100111d11ss10dddd00000qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VSWP_111100111d11ss10dddd00000qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(22):inst(15:12)  ==
  //          inst(5):inst(3:0) => UNKNOWN
  if ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) == ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))))
    return UNKNOWN;

  // inst(19:18)=~00 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  !=
          0x00000000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VSWP_111100111d11ss10dddd00000qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_1,
//    baseline: VectorBinary3RegisterLookupOp,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [D(22),
//      Vn(19:16),
//      Vd(15:12),
//      len(9:8),
//      N(7),
//      op(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0,
//    is_vtbl: op(6)=0,
//    len: len(9:8),
//    length: len + 1,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(6),
//    pattern: 111100111d11nnnndddd10ccnpm0mmmm,
//    rule: VTBL_VTBX,
//    safety: [n + length  >
//            32 => UNPREDICTABLE],
//    uses: {}}
RegisterList VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 32  <=
  //          inst(7):inst(19:16) + inst(9:8) + 1 => UNPREDICTABLE
  if ((((((((inst.Bits() & 0x00000080) >> 7)) << 4) | ((inst.Bits() & 0x000F0000) >> 16)) + ((inst.Bits() & 0x00000300) >> 8) + 1) > (32)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList VTBL_VTBX_111100111d11nnnndddd10ccnpm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VTRN_111100111d11ss10dddd00001qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32T,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VTRN_111100111d11ss10dddd00001qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss10dddd00001qm0mmmm,
//    quadword_operation: Q(6)=1,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VTRN,
//    safety: [d  ==
//            m => UNKNOWN,
//      size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VTRN_111100111d11ss10dddd00001qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VTRN_111100111d11ss10dddd00001qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(22):inst(15:12)  ==
  //          inst(5):inst(3:0) => UNKNOWN
  if ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) == ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))))
    return UNKNOWN;

  // inst(19:18)=11 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  ==
          0x000C0000)
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VTRN_111100111d11ss10dddd00001qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VTST_111100100dssnnnndddd1000nqm1mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    actual: Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1,
//    arch: ASIMD,
//    baseline: VectorBinary3RegisterSameLengthDQI8_16_32,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [U(24),
//      D(22),
//      size(21:20),
//      Vn(19:16),
//      Vd(15:12),
//      op(9),
//      N(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VTST_111100100dssnnnndddd1000nqm1mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100dssnnnndddd1000nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VTST,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
RegisterList VTST_111100100dssnnnndddd1000nqm1mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VTST_111100100dssnnnndddd1000nqm1mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VTST_111100100dssnnnndddd1000nqm1mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VUZP_111100111d11ss10dddd00010qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VUZP_111100111d11ss10dddd00010qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss10dddd00010qm0mmmm,
//    quadword_operation: Q(6)=1,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VUZP,
//    safety: [d  ==
//            m => UNKNOWN,
//      size(19:18)=11 ||
//         (Q(6)=0 &&
//         size(19:18)=10) => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VUZP_111100111d11ss10dddd00010qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VUZP_111100111d11ss10dddd00010qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(22):inst(15:12)  ==
  //          inst(5):inst(3:0) => UNKNOWN
  if ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) == ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))))
    return UNKNOWN;

  // inst(19:18)=11 ||
  //       (inst(6)=0 &&
  //       inst(19:18)=10) => UNDEFINED
  if (((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       ((((inst.Bits() & 0x00000040)  ==
          0x00000000) &&
       ((inst.Bits() & 0x000C0000)  ==
          0x00080000))))
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VUZP_111100111d11ss10dddd00010qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// VZIP_111100111d11ss10dddd00011qm0mmmm_case_0:
//
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    actual: Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1,
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_V8_16_32I,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 64 / esize,
//    esize: 8 << size,
//    fields: [D(22),
//      size(19:18),
//      Vd(15:12),
//      F(10),
//      op(8:7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    generated_baseline: VZIP_111100111d11ss10dddd00011qm0mmmm_case_0,
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss10dddd00011qm0mmmm,
//    quadword_operation: Q(6)=1,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VZIP,
//    safety: [d  ==
//            m => UNKNOWN,
//      size(19:18)=11 ||
//         (Q(6)=0 &&
//         size(19:18)=10) => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
RegisterList VZIP_111100111d11ss10dddd00011qm0mmmm_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel VZIP_111100111d11ss10dddd00011qm0mmmm_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(22):inst(15:12)  ==
  //          inst(5):inst(3:0) => UNKNOWN
  if ((((((((inst.Bits() & 0x00400000) >> 22)) << 4) | ((inst.Bits() & 0x0000F000) >> 12))) == ((((((inst.Bits() & 0x00000020) >> 5)) << 4) | (inst.Bits() & 0x0000000F)))))
    return UNKNOWN;

  // inst(19:18)=11 ||
  //       (inst(6)=0 &&
  //       inst(19:18)=10) => UNDEFINED
  if (((inst.Bits() & 0x000C0000)  ==
          0x000C0000) ||
       ((((inst.Bits() & 0x00000040)  ==
          0x00000000) &&
       ((inst.Bits() & 0x000C0000)  ==
          0x00080000))))
    return UNDEFINED;

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(3:0)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList VZIP_111100111d11ss10dddd00011qm0mmmm_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// WFE_cccc0011001000001111000000000010_case_0:
//
//   {actual: Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1,
//    arch: v6K,
//    baseline: WFE_cccc0011001000001111000000000010_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000010,
//    rule: WFE,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
RegisterList WFE_cccc0011001000001111000000000010_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel WFE_cccc0011001000001111000000000010_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => FORBIDDEN
  if (true)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList WFE_cccc0011001000001111000000000010_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// WFI_cccc0011001000001111000000000011_case_0:
//
//   {actual: Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1,
//    arch: v6K,
//    baseline: WFI_cccc0011001000001111000000000011_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000011,
//    rule: WFI,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
RegisterList WFI_cccc0011001000001111000000000011_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel WFI_cccc0011001000001111000000000011_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => FORBIDDEN
  if (true)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList WFI_cccc0011001000001111000000000011_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// YIELD_cccc0011001000001111000000000001_case_0:
//
//   {actual: Actual_NOP_cccc0011001000001111000000000000_case_1,
//    arch: v6K,
//    baseline: YIELD_cccc0011001000001111000000000001_case_0,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000001,
//    rule: YIELD,
//    uses: {}}
RegisterList YIELD_cccc0011001000001111000000000001_case_0::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel YIELD_cccc0011001000001111000000000001_case_0::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  return MAY_BE_SAFE;
}


RegisterList YIELD_cccc0011001000001111000000000001_case_0::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

}  // namespace nacl_arm_dec
