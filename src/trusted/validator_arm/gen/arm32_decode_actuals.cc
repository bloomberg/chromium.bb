/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#include "native_client/src/trusted/validator_arm/inst_classes.h"
#include "native_client/src/trusted/validator_arm/gen/arm32_decode_actuals.h"

namespace nacl_arm_dec {

// Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16)}}

RegisterList Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)
//         else 32},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16), inst(3:0)}}

RegisterList Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)));
}

SafetyLevel Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    safety: [15  ==
//            inst(19:16) ||
//         15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE],
//    uses: {inst(19:16), inst(3:0), inst(11:8)}}

RegisterList Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      (inst(19:16)=1111 &&
//         inst(20)=0) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16)}}

RegisterList Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // (inst(19:16)=1111 &&
  //       inst(20)=0) => DECODER_ERROR
  if ((((inst.Bits() & 0x000F0000)  ==
          0x000F0000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00000000)))
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {15}}

RegisterList Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)
//         else 32},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(3:0)}}

RegisterList Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)));
}

SafetyLevel Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(11:8)}}

RegisterList Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(20:16)  <
//            inst(11:7) => UNPREDICTABLE],
//    uses: {inst(15:12)}}

RegisterList Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  // inst(20:16)  <
  //          inst(11:7) => UNPREDICTABLE
  if (((((inst.Bits() & 0x001F0000) >> 16)) < (((inst.Bits() & 0x00000F80) >> 7))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      15  ==
//            inst(3:0) => DECODER_ERROR,
//      inst(20:16)  <
//            inst(11:7) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(15:12)}}

RegisterList Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(3:0) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000000F)) == (15)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  // inst(20:16)  <
  //          inst(11:7) => UNPREDICTABLE
  if (((((inst.Bits() & 0x001F0000) >> 16)) < (((inst.Bits() & 0x00000F80) >> 7))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(15:12)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1
//
// Actual:
//   {defs: {},
//    pool_head: true,
//    safety: [inst(31:28)=~1110 => UNPREDICTABLE,
//      not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS],
//    uses: {}}

RegisterList Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

bool Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1::
is_literal_pool_head(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // pool_head: 'true'
  return true;
}

SafetyLevel Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(31:28)=~1110 => UNPREDICTABLE
  if ((inst.Bits() & 0xF0000000)  !=
          0xE0000000)
    return UNPREDICTABLE;

  // not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS
  if (!(nacl_arm_dec::IsBreakPointAndConstantPoolHead(inst.Bits())))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_BLX_register_cccc000100101111111111110011mmmm_case_1
//
// Actual:
//   {defs: {15, 14},
//    safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS],
//    target: inst(3:0),
//    uses: {inst(3:0)}}

RegisterList Actual_BLX_register_cccc000100101111111111110011mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{15, 14}'
  return RegisterList().
   Add(Register(15)).
   Add(Register(14));
}

SafetyLevel Actual_BLX_register_cccc000100101111111111110011mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(3:0)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000000F)  ==
          0x0000000F)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


Register Actual_BLX_register_cccc000100101111111111110011mmmm_case_1::
branch_target_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // target: 'inst(3:0)'
  return Register((inst.Bits() & 0x0000000F));
}

RegisterList Actual_BLX_register_cccc000100101111111111110011mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {15, 14},
//    relative: true,
//    relative_offset: SignExtend(inst(23:0):0(1:0), 32),
//    safety: [true => MAY_BE_SAFE],
//    uses: {15}}

RegisterList Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{15, 14}'
  return RegisterList().
   Add(Register(15)).
   Add(Register(14));
}

bool Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1::
is_relative_branch(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // relative: 'true'
  return true;
}

int32_t Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1::
branch_target_offset(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // relative_offset: 'SignExtend(inst(23:0):0(1:0), 32)'
  return (((((((inst.Bits() & 0x00FFFFFF)) << 2) | (0 & 0x00000003))) & 0x02000000)
       ? ((((((inst.Bits() & 0x00FFFFFF)) << 2) | (0 & 0x00000003))) | 0xFC000000)
       : ((((inst.Bits() & 0x00FFFFFF)) << 2) | (0 & 0x00000003)));
}

SafetyLevel Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => MAY_BE_SAFE
  if (true)
    return MAY_BE_SAFE;

  return MAY_BE_SAFE;
}


RegisterList Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {15},
//    relative: true,
//    relative_offset: SignExtend(inst(23:0):0(1:0), 32),
//    safety: [true => MAY_BE_SAFE],
//    uses: {15}}

RegisterList Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{15}'
  return RegisterList().
   Add(Register(15));
}

bool Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1::
is_relative_branch(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // relative: 'true'
  return true;
}

int32_t Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1::
branch_target_offset(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // relative_offset: 'SignExtend(inst(23:0):0(1:0), 32)'
  return (((((((inst.Bits() & 0x00FFFFFF)) << 2) | (0 & 0x00000003))) & 0x02000000)
       ? ((((((inst.Bits() & 0x00FFFFFF)) << 2) | (0 & 0x00000003))) | 0xFC000000)
       : ((((inst.Bits() & 0x00FFFFFF)) << 2) | (0 & 0x00000003)));
}

SafetyLevel Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // true => MAY_BE_SAFE
  if (true)
    return MAY_BE_SAFE;

  return MAY_BE_SAFE;
}


RegisterList Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_Bx_cccc000100101111111111110001mmmm_case_1
//
// Actual:
//   {defs: {15},
//    safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS],
//    target: inst(3:0),
//    uses: {inst(3:0)}}

RegisterList Actual_Bx_cccc000100101111111111110001mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{15}'
  return RegisterList().
   Add(Register(15));
}

SafetyLevel Actual_Bx_cccc000100101111111111110001mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(3:0)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000000F)  ==
          0x0000000F)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


Register Actual_Bx_cccc000100101111111111110001mmmm_case_1::
branch_target_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // target: 'inst(3:0)'
  return Register((inst.Bits() & 0x0000000F));
}

RegisterList Actual_Bx_cccc000100101111111111110001mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE],
//    uses: {inst(3:0)}}

RegisterList Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {16},
//    uses: {inst(19:16)}}

RegisterList Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{16}'
  return RegisterList().
   Add(Register(16));
}

RegisterList Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1
//
// Actual:
//   {defs: {16
//         if inst(20)
//         else 32},
//    uses: {inst(19:16), inst(3:0)}}

RegisterList Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{16
  //       if inst(20)
  //       else 32}'
  return RegisterList().
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)));
}

RegisterList Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1
//
// Actual:
//   {defs: {16},
//    safety: [15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE],
//    uses: {inst(19:16), inst(3:0), inst(11:8)}}

RegisterList Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{16}'
  return RegisterList().
   Add(Register(16));
}

SafetyLevel Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1
//
// Actual:
//   {safety: [inst(19:18)=~01 => UNDEFINED,
//      inst(8)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED,
//      not inst(8)=1 &&
//         inst(3:0)(0)=1 => UNDEFINED]}

SafetyLevel Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=~01 => UNDEFINED
  if ((inst.Bits() & 0x000C0000)  !=
          0x00040000)
    return UNDEFINED;

  // inst(8)=1 &&
  //       inst(15:12)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00000100)  ==
          0x00000100) &&
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  // not inst(8)=1 &&
  //       inst(3:0)(0)=1 => UNDEFINED
  if ((!((inst.Bits() & 0x00000100)  ==
          0x00000100)) &&
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


// Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: Union({inst(19:16)
//         if inst(21)=1
//         else 32}, RegisterList(inst(15:0))),
//    safety: [15  ==
//            inst(19:16) ||
//         NumGPRs(RegisterList(inst(15:0)))  <
//            1 => UNPREDICTABLE,
//      Contains(RegisterList(inst(15:0)), 15) => FORBIDDEN_OPERANDS,
//      inst(21)=1 &&
//         Contains(RegisterList(inst(15:0)), inst(19:16)) => UNKNOWN],
//    small_imm_base_wb: true,
//    uses: {inst(19:16)}}

Register Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: 'Union({inst(19:16)
  //       if inst(21)=1
  //       else 32}, RegisterList(inst(15:0)))'
  return nacl_arm_dec::Union(RegisterList().
   Add(Register(((inst.Bits() & 0x00200000)  ==
          0x00200000
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32))), nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)));
}

SafetyLevel Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       NumGPRs(RegisterList(inst(15:0)))  <
  //          1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1))))
    return UNPREDICTABLE;

  // inst(21)=1 &&
  //       Contains(RegisterList(inst(15:0)), inst(19:16)) => UNKNOWN
  if (((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16)))))
    return UNKNOWN;

  // Contains(RegisterList(inst(15:0)), 15) => FORBIDDEN_OPERANDS
  if (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


bool Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      15  ==
//            inst(19:16) => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    small_imm_base_wb: inst(24)=0 ||
//         inst(21)=1,
//    uses: {inst(19:16)}}

Register Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) => DECODER_ERROR
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return DECODER_ERROR;

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(24)=0 ||
  //       inst(21)=1'
  return ((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000);
}

RegisterList Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: 15,
//    defs: {inst(15:12)},
//    is_literal_load: true,
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {15}}

Register Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '15'
  return Register(15);
}

RegisterList Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

bool Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: 'true'
  return true;
}

SafetyLevel Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         inst(24)=0 ||
//         inst(21)=1 &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(3:0), inst(19:16)}}

Register Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       inst(24)=0 ||
  //       inst(21)=1 &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(19:16)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(15:12) + 1, inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (inst(15:12)  ==
//            inst(19:16) ||
//         inst(15:12) + 1  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) + 1 => UNPREDICTABLE,
//      inst(15:12)(0)=1 => UNPREDICTABLE,
//      inst(19:16)=1111 => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(21)=1 => UNPREDICTABLE],
//    small_imm_base_wb: (inst(24)=0) ||
//         (inst(21)=1),
//    uses: {inst(19:16)}}

Register Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(15:12) + 1, inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1)).
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:16)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000)
    return DECODER_ERROR;

  // inst(15:12)(0)=1 => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNPREDICTABLE;

  // inst(24)=0 &&
  //       inst(21)=1 => UNPREDICTABLE
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (inst(15:12)  ==
  //          inst(19:16) ||
  //       inst(15:12) + 1  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12) + 1))))))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) + 1 => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(inst(24)=0) ||
  //       (inst(21)=1)'
  return (((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000));
}

RegisterList Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1
//
// Actual:
//   {base: 15,
//    defs: {inst(15:12), inst(15:12) + 1},
//    is_literal_load: true,
//    safety: [15  ==
//            inst(15:12) + 1 => UNPREDICTABLE,
//      inst(15:12)(0)=1 => UNPREDICTABLE],
//    uses: {15}}

Register Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '15'
  return Register(15);
}

RegisterList Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(15:12) + 1}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1));
}

bool Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: 'true'
  return true;
}

SafetyLevel Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)(0)=1 => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) + 1 => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(15:12) + 1, inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16) ||
//         inst(15:12) + 1  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) + 1 ||
//         15  ==
//            inst(3:0) ||
//         inst(15:12)  ==
//            inst(3:0) ||
//         inst(15:12) + 1  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         (inst(24)=0) ||
//         (inst(21)=1) &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(15:12)(0)=1 => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => UNPREDICTABLE,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(19:16), inst(3:0)}}

Register Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(15:12) + 1, inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1)).
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)(0)=1 => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNPREDICTABLE;

  // inst(24)=0 &&
  //       inst(21)=1 => UNPREDICTABLE
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) + 1 ||
  //       15  ==
  //          inst(3:0) ||
  //       inst(15:12)  ==
  //          inst(3:0) ||
  //       inst(15:12) + 1  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15))) ||
       ((((inst.Bits() & 0x0000000F)) == (15))) ||
       ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x0000F000) >> 12) + 1))))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16) ||
  //       inst(15:12) + 1  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12) + 1))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       ((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {inst(19:16)}}

Register Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(15:12) + 1},
//    safety: [inst(15:12)(0)=1 ||
//         14  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {inst(19:16)}}

Register Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(15:12) + 1}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1));
}

SafetyLevel Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)(0)=1 ||
  //       14  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       (((((inst.Bits() & 0x0000F000) >> 12)) == (14))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (15))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [15  ==
//            inst(15:12) => FORBIDDEN_OPERANDS,
//      15  ==
//            inst(15:12) ||
//         ((inst(24)=0) ||
//         (inst(21)=1) &&
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      inst(19:16)=1111 => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR],
//    small_imm_base_wb: (inst(24)=0) ||
//         (inst(21)=1),
//    uses: {inst(19:16)}}

Register Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:16)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000)
    return DECODER_ERROR;

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) ||
  //       ((inst(24)=0) ||
  //       (inst(21)=1) &&
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if ((((((inst.Bits() & 0x0000F000) >> 12)) == (15))) ||
       ((((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


bool Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(inst(24)=0) ||
  //       (inst(21)=1)'
  return (((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000));
}

RegisterList Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1
//
// Actual:
//   {base: 15,
//    defs: {inst(15:12)},
//    is_literal_load: true,
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(21)  ==
//            inst(24) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR],
//    uses: {15}}

Register Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '15'
  return Register(15);
}

RegisterList Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

bool Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: 'true'
  return true;
}

SafetyLevel Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // inst(21)  ==
  //          inst(24) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000) >> 24)) == (((inst.Bits() & 0x00200000) >> 21))))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         (inst(24)=0) ||
//         (inst(21)=1) &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(19:16), inst(3:0)}}

Register Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       ((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    is_load_tp: 9  ==
//            inst(19:16) &&
//         inst(24)=1 &&
//         not inst(24)=0 ||
//         inst(21)=1 &&
//         inst(23)=1 &&
//         0  ==
//            inst(11:0) ||
//         4  ==
//            inst(11:0),
//    safety: [15  ==
//            inst(15:12) => FORBIDDEN_OPERANDS,
//      15  ==
//            inst(19:16) => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    small_imm_base_wb: inst(24)=0 ||
//         inst(21)=1,
//    uses: {inst(19:16)}}

Register Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

bool Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1::
is_load_thread_address_pointer(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_load_tp: '9  ==
  //          inst(19:16) &&
  //       inst(24)=1 &&
  //       not inst(24)=0 ||
  //       inst(21)=1 &&
  //       inst(23)=1 &&
  //       0  ==
  //          inst(11:0) ||
  //       4  ==
  //          inst(11:0)'
  return (((((inst.Bits() & 0x000F0000) >> 16)) == (9))) &&
       ((inst.Bits() & 0x01000000)  ==
          0x01000000) &&
       (!(((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       ((inst.Bits() & 0x00800000)  ==
          0x00800000) &&
       (((((inst.Bits() & 0x00000FFF)) == (0))) ||
       ((((inst.Bits() & 0x00000FFF)) == (4))));
}

SafetyLevel Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) => DECODER_ERROR
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return DECODER_ERROR;

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


bool Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(24)=0 ||
  //       inst(21)=1'
  return ((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000);
}

RegisterList Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: 15,
//    defs: {inst(15:12)},
//    is_literal_load: true,
//    uses: {15}}

Register Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '15'
  return Register(15);
}

RegisterList Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

bool Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: 'true'
  return true;
}

RegisterList Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{15}'
  return RegisterList().
   Add(Register(15));
}

// Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12), inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    safety: [15  ==
//            inst(15:12) => FORBIDDEN_OPERANDS,
//      15  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         inst(24)=0 ||
//         inst(21)=1 &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(3:0), inst(19:16)}}

Register Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000000F)) == (15)))
    return UNPREDICTABLE;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       inst(24)=0 ||
  //       inst(21)=1 &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  // 15  ==
  //          inst(15:12) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(19:16)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)
//         else 32},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(11:7)=00000 => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(3:0)}}

RegisterList Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)));
}

SafetyLevel Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // inst(11:7)=00000 => DECODER_ERROR
  if ((inst.Bits() & 0x00000F80)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16), 16
//         if inst(20)=1
//         else 32},
//    safety: [(ArchVersion()  <
//            6 &&
//         inst(19:16)  ==
//            inst(3:0)) => UNPREDICTABLE,
//      15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) ||
//         15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(11:8), inst(15:12)}}

RegisterList Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) ||
  //       15  ==
  //          inst(15:12) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12)))))
    return UNPREDICTABLE;

  // (ArchVersion()  <
  //          6 &&
  //       inst(19:16)  ==
  //          inst(3:0)) => UNPREDICTABLE
  if (((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(11:8), inst(15:12)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16)},
//    safety: [15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) ||
//         15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(11:8), inst(15:12)}}

RegisterList Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

SafetyLevel Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) ||
  //       15  ==
  //          inst(15:12) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(11:8), inst(15:12)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(23):inst(22:21):inst(6:5)(4:0)=10x00 ||
//         inst(23):inst(22:21):inst(6:5)(4:0)=x0x10 => UNDEFINED]}

RegisterList Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(23):inst(22:21):inst(6:5)(4:0)=10x00 ||
  //       inst(23):inst(22:21):inst(6:5)(4:0)=x0x10 => UNDEFINED
  if (((((((((((inst.Bits() & 0x00800000) >> 23)) << 2) | ((inst.Bits() & 0x00600000) >> 21))) << 2) | ((inst.Bits() & 0x00000060) >> 5)) & 0x0000001B)  ==
          0x00000010) ||
       ((((((((((inst.Bits() & 0x00800000) >> 23)) << 2) | ((inst.Bits() & 0x00600000) >> 21))) << 2) | ((inst.Bits() & 0x00000060) >> 5)) & 0x0000000B)  ==
          0x00000002))
    return UNDEFINED;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


// Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)
//         else 32},
//    dynamic_code_replace_immediates: {inst(19:16), inst(11:0)},
//    safety: [inst(15:12)=1111 => UNPREDICTABLE],
//    uses: {}}

RegisterList Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((((inst.Bits() & 0x00100000) >> 20) != 0)
       ? 16
       : 32)));
}

Instruction Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1::
dynamic_code_replacement_sentinel(
     Instruction inst) const {
  if (!defs(inst).ContainsAny(RegisterList::DynCodeReplaceFrozenRegs())) {
    // inst(19:16)
    inst.SetBits(19, 16, 0);
    // inst(11:0)
    inst.SetBits(11, 0, 0);
  }
  return inst;
}

SafetyLevel Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)=1111 => UNPREDICTABLE
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    dynamic_code_replace_immediates: {inst(11:0)},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {}}

RegisterList Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

Instruction Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1::
dynamic_code_replacement_sentinel(
     Instruction inst) const {
  if (!defs(inst).ContainsAny(RegisterList::DynCodeReplaceFrozenRegs())) {
    // inst(11:0)
    inst.SetBits(11, 0, 0);
  }
  return inst;
}

SafetyLevel Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_MRS_cccc00010r001111dddd000000000000_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [inst(15:12)=1111 => UNPREDICTABLE,
//      inst(22)=1 => FORBIDDEN_OPERANDS],
//    uses: {}}

RegisterList Actual_MRS_cccc00010r001111dddd000000000000_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_MRS_cccc00010r001111dddd000000000000_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(22)=1 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x00400000)  ==
          0x00400000)
    return FORBIDDEN_OPERANDS;

  // inst(15:12)=1111 => UNPREDICTABLE
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_MRS_cccc00010r001111dddd000000000000_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1
//
// Actual:
//   {defs: {16
//         if inst(19:18)(1)=1
//         else 32},
//    safety: [15  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(19:18)=00 => UNPREDICTABLE],
//    uses: {inst(3:0)}}

RegisterList Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{16
  //       if inst(19:18)(1)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((((inst.Bits() & 0x000C0000) >> 18) & 0x00000002)  ==
          0x00000002
       ? 16
       : 32)));
}

SafetyLevel Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:18)=00 => UNPREDICTABLE
  if ((inst.Bits() & 0x000C0000)  ==
          0x00000000)
    return UNPREDICTABLE;

  // 15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000000F)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16), 16
//         if inst(20)=1
//         else 32},
//    safety: [(ArchVersion()  <
//            6 &&
//         inst(19:16)  ==
//            inst(3:0)) => UNPREDICTABLE,
//      15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE],
//    uses: {inst(11:8), inst(3:0)}}

RegisterList Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  // (ArchVersion()  <
  //          6 &&
  //       inst(19:16)  ==
  //          inst(3:0)) => UNPREDICTABLE
  if (((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(11:8), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00000F00) >> 8))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    dynamic_code_replace_immediates: {inst(11:0)},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16)}}

RegisterList Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

Instruction Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1::
dynamic_code_replacement_sentinel(
     Instruction inst) const {
  if (!defs(inst).ContainsAny(RegisterList::DynCodeReplaceFrozenRegs())) {
    // inst(11:0)
    inst.SetBits(11, 0, 0);
  }
  return inst;
}

SafetyLevel Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // (inst(15:12)=1111 &&
  //       inst(20)=1) => DECODER_ERROR
  if ((((inst.Bits() & 0x0000F000)  ==
          0x0000F000) &&
       ((inst.Bits() & 0x00100000)  ==
          0x00100000)))
    return DECODER_ERROR;

  // inst(15:12)=1111 => FORBIDDEN_OPERANDS
  if ((inst.Bits() & 0x0000F000)  ==
          0x0000F000)
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE],
//    uses: {inst(19:16), inst(3:0)}}

RegisterList Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE,
//      31  <=
//            inst(11:7) + inst(20:16) => UNPREDICTABLE],
//    uses: {inst(3:0)}}

RegisterList Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // 31  <=
  //          inst(11:7) + inst(20:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x00000F80) >> 7) + ((inst.Bits() & 0x001F0000) >> 16)) > (31)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16)},
//    safety: [15  ==
//            inst(19:16) ||
//         15  ==
//            inst(11:8) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE],
//    uses: {inst(11:8), inst(3:0)}}

RegisterList Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

SafetyLevel Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(11:8) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(11:8), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x00000F00) >> 8))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16)},
//    safety: [15  ==
//            inst(15:12) => DECODER_ERROR,
//      15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(11:8), inst(15:12)}}

RegisterList Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

SafetyLevel Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => DECODER_ERROR
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return DECODER_ERROR;

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(11:8), inst(15:12)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1
//
// Actual:
//   {defs: {inst(15:12), inst(19:16)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE,
//      inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {inst(15:12), inst(19:16), inst(3:0), inst(11:8)}}

RegisterList Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

SafetyLevel Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  // inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12), inst(19:16), inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16), inst(15:12)},
//    safety: [15  ==
//            inst(19:16) ||
//         15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE,
//      inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {inst(19:16), inst(15:12), inst(11:8), inst(3:0)}}

RegisterList Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16), inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  // inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(15:12), inst(11:8), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1
//
// Actual:
//   {defs: {inst(15:12), inst(19:16), 16
//         if inst(20)=1
//         else 32},
//    safety: [(ArchVersion()  <
//            6 &&
//         (inst(19:16)  ==
//            inst(3:0) ||
//         inst(15:12)  ==
//            inst(3:0))) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE,
//      inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {inst(15:12), inst(19:16), inst(3:0), inst(11:8)}}

RegisterList Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  // inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))
    return UNPREDICTABLE;

  // (ArchVersion()  <
  //          6 &&
  //       (inst(19:16)  ==
  //          inst(3:0) ||
  //       inst(15:12)  ==
  //          inst(3:0))) => UNPREDICTABLE
  if (((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F)))) ||
       (((((inst.Bits() & 0x0000F000) >> 12)) == ((inst.Bits() & 0x0000000F))))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12), inst(19:16), inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1
//
// Actual:
//   {defs: {inst(19:16)},
//    safety: [15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(11:8)}}

RegisterList Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

SafetyLevel Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1
//
// Actual:
//   {defs: {inst(15:12), inst(19:16), 16
//         if inst(20)=1
//         else 32},
//    safety: [(ArchVersion()  <
//            6 &&
//         (inst(19:16)  ==
//            inst(3:0) ||
//         inst(15:12)  ==
//            inst(3:0))) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(11:8) => UNPREDICTABLE,
//      inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {inst(3:0), inst(11:8)}}

RegisterList Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12), inst(19:16), 16
  //       if inst(20)=1
  //       else 32}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x00100000)  ==
          0x00100000
       ? 16
       : 32)));
}

SafetyLevel Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(11:8) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x00000F00) >> 8)))))
    return UNPREDICTABLE;

  // inst(15:12)  ==
  //          inst(19:16) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12))))
    return UNPREDICTABLE;

  // (ArchVersion()  <
  //          6 &&
  //       (inst(19:16)  ==
  //          inst(3:0) ||
  //       inst(15:12)  ==
  //          inst(3:0))) => UNPREDICTABLE
  if (((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F)))) ||
       (((((inst.Bits() & 0x0000F000) >> 12)) == ((inst.Bits() & 0x0000000F))))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(11:8)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x00000F00) >> 8)));
}

// Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(21)=1
//         else 32},
//    safety: [15  ==
//            inst(19:16) ||
//         NumGPRs(RegisterList(inst(15:0)))  <
//            1 => UNPREDICTABLE,
//      inst(21)=1 &&
//         Contains(RegisterList(inst(15:0)), inst(19:16)) &&
//         SmallestGPR(RegisterList(inst(15:0)))  !=
//            inst(19:16) => UNKNOWN],
//    small_imm_base_wb: true,
//    uses: Union({inst(19:16)}, RegisterList(inst(15:0)))}

Register Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1::
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

SafetyLevel Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) ||
  //       NumGPRs(RegisterList(inst(15:0)))  <
  //          1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((nacl_arm_dec::NumGPRs(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))) < (1))))
    return UNPREDICTABLE;

  // inst(21)=1 &&
  //       Contains(RegisterList(inst(15:0)), inst(19:16)) &&
  //       SmallestGPR(RegisterList(inst(15:0)))  !=
  //          inst(19:16) => UNKNOWN
  if (((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       (nacl_arm_dec::Contains(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)), Register(((inst.Bits() & 0x000F0000) >> 16)))) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) != (nacl_arm_dec::SmallestGPR(nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)))))))
    return UNKNOWN;

  return MAY_BE_SAFE;
}


bool Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: 'Union({inst(19:16)}, RegisterList(inst(15:0)))'
  return nacl_arm_dec::Union(RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))), nacl_arm_dec::RegisterList((inst.Bits() & 0x0000FFFF)));
}

// Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE],
//    small_imm_base_wb: inst(24)=0 ||
//         inst(21)=1,
//    uses: {inst(19:16), inst(15:12)}}

Register Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(24)=0 ||
  //       inst(21)=1'
  return ((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000);
}

RegisterList Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    safety: [15  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         inst(24)=0 ||
//         inst(21)=1 &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(3:0), inst(19:16), inst(15:12)}}

Register Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000000F)) == (15)))
    return UNPREDICTABLE;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       inst(24)=0 ||
  //       inst(21)=1 &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       (((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((inst.Bits() & 0x000F0000) >> 16)) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(3:0), inst(19:16), inst(15:12)}'
  return RegisterList().
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) + 1 => UNPREDICTABLE,
//      inst(15:12)(0)=1 => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => UNPREDICTABLE],
//    small_imm_base_wb: (inst(24)=0) ||
//         (inst(21)=1),
//    uses: {inst(15:12), inst(15:12) + 1, inst(19:16)}}

Register Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)(0)=1 => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNPREDICTABLE;

  // inst(24)=0 &&
  //       inst(21)=1 => UNPREDICTABLE
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) + 1 => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(inst(24)=0) ||
  //       (inst(21)=1)'
  return (((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000));
}

RegisterList Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12), inst(15:12) + 1, inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1)).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16) ||
//         inst(15:12) + 1  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) + 1 ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         (inst(24)=0) ||
//         (inst(21)=1) &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(15:12)(0)=1 => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => UNPREDICTABLE,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(15:12), inst(15:12) + 1, inst(19:16), inst(3:0)}}

Register Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(15:12)(0)=1 => UNPREDICTABLE
  if ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001)
    return UNPREDICTABLE;

  // inst(24)=0 &&
  //       inst(21)=1 => UNPREDICTABLE
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return UNPREDICTABLE;

  // 15  ==
  //          inst(15:12) + 1 ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((((inst.Bits() & 0x0000F000) >> 12) + 1) == (15))) ||
       ((((inst.Bits() & 0x0000000F)) == (15))))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16) ||
  //       inst(15:12) + 1  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12) + 1))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       ((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12), inst(15:12) + 1, inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12) + 1)).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) ||
//         15  ==
//            inst(19:16) => UNPREDICTABLE,
//      inst(15:12)  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(3:0) => UNPREDICTABLE],
//    uses: {inst(19:16), inst(3:0)}}

Register Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) ||
  //       15  ==
  //          inst(19:16) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  // inst(15:12)  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((((inst.Bits() & 0x0000F000) >> 12)) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((((inst.Bits() & 0x0000F000) >> 12)) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) ||
//         inst(3:0)(0)=1 ||
//         14  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(15:12)  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(3:0) ||
//         inst(15:12)  ==
//            inst(3:0) + 1 => UNPREDICTABLE],
//    uses: {inst(19:16), inst(3:0), inst(3:0) + 1}}

Register Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(19:16) ||
  //       inst(3:0)(0)=1 ||
  //       14  ==
  //          inst(3:0) => UNPREDICTABLE
  if (((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == (((inst.Bits() & 0x000F0000) >> 16))))) ||
       (((inst.Bits() & 0x0000000F) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000000F)) == (14))))
    return UNPREDICTABLE;

  // inst(15:12)  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(3:0) ||
  //       inst(15:12)  ==
  //          inst(3:0) + 1 => UNPREDICTABLE
  if ((((((inst.Bits() & 0x0000F000) >> 12)) == (((inst.Bits() & 0x000F0000) >> 16)))) ||
       (((((inst.Bits() & 0x0000F000) >> 12)) == ((inst.Bits() & 0x0000000F)))) ||
       (((((inst.Bits() & 0x0000F000) >> 12)) == ((inst.Bits() & 0x0000000F) + 1))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0), inst(3:0) + 1}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F))).
   Add(Register((inst.Bits() & 0x0000000F) + 1));
}

// Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR],
//    small_imm_base_wb: (inst(24)=0) ||
//         (inst(21)=1),
//    uses: {inst(15:12), inst(19:16)}}

Register Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: '(inst(24)=0) ||
  //       (inst(21)=1)'
  return (((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000));
}

RegisterList Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12), inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if (inst(24)=0) ||
//         (inst(21)=1)
//         else 32},
//    safety: [(inst(24)=0) ||
//         (inst(21)=1) &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE,
//      15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         (inst(24)=0) ||
//         (inst(21)=1) &&
//         inst(19:16)  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=1 => FORBIDDEN],
//    uses: {inst(15:12), inst(19:16), inst(3:0)}}

Register Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if (inst(24)=0) ||
  //       (inst(21)=1)
  //       else 32}'
  return RegisterList().
   Add(Register(((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  // (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if (((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  // ArchVersion()  <
  //          6 &&
  //       (inst(24)=0) ||
  //       (inst(21)=1) &&
  //       inst(19:16)  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((nacl_arm_dec::ArchVersion()) < (6))) &&
       ((((inst.Bits() & 0x01000000)  ==
          0x00000000)) ||
       (((inst.Bits() & 0x00200000)  ==
          0x00200000))) &&
       ((((inst.Bits() & 0x0000000F)) == (((inst.Bits() & 0x000F0000) >> 16)))))
    return UNPREDICTABLE;

  // inst(24)=1 => FORBIDDEN
  if ((inst.Bits() & 0x01000000)  ==
          0x01000000)
    return FORBIDDEN;

  return MAY_BE_SAFE;
}


RegisterList Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12), inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12))).
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(24)=0 ||
//         inst(21)=1
//         else 32},
//    safety: [inst(24)=0 &&
//         inst(21)=1 => DECODER_ERROR,
//      inst(24)=0 ||
//         inst(21)=1 &&
//         (15  ==
//            inst(19:16) ||
//         inst(15:12)  ==
//            inst(19:16)) => UNPREDICTABLE],
//    small_imm_base_wb: inst(24)=0 ||
//         inst(21)=1,
//    uses: {inst(19:16), inst(15:12)}}

Register Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(19:16)
  //       if inst(24)=0 ||
  //       inst(21)=1
  //       else 32}'
  return RegisterList().
   Add(Register((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)
       ? ((inst.Bits() & 0x000F0000) >> 16)
       : 32)));
}

SafetyLevel Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(24)=0 &&
  //       inst(21)=1 => DECODER_ERROR
  if (((inst.Bits() & 0x01000000)  ==
          0x00000000) &&
       ((inst.Bits() & 0x00200000)  ==
          0x00200000))
    return DECODER_ERROR;

  // inst(24)=0 ||
  //       inst(21)=1 &&
  //       (15  ==
  //          inst(19:16) ||
  //       inst(15:12)  ==
  //          inst(19:16)) => UNPREDICTABLE
  if ((((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000)) &&
       (((((((inst.Bits() & 0x000F0000) >> 16)) == (15))) ||
       (((((inst.Bits() & 0x000F0000) >> 16)) == (((inst.Bits() & 0x0000F000) >> 12)))))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(24)=0 ||
  //       inst(21)=1'
  return ((inst.Bits() & 0x01000000)  ==
          0x00000000) ||
       ((inst.Bits() & 0x00200000)  ==
          0x00200000);
}

RegisterList Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE,
//      inst(19:16)=1111 => DECODER_ERROR],
//    uses: {inst(19:16), inst(3:0)}}

RegisterList Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

SafetyLevel Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(19:16)=1111 => DECODER_ERROR
  if ((inst.Bits() & 0x000F0000)  ==
          0x000F0000)
    return DECODER_ERROR;

  // 15  ==
  //          inst(15:12) ||
  //       15  ==
  //          inst(3:0) => UNPREDICTABLE
  if ((((15) == (((inst.Bits() & 0x0000F000) >> 12)))) ||
       (((15) == ((inst.Bits() & 0x0000000F)))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16), inst(3:0)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16))).
   Add(Register((inst.Bits() & 0x0000000F)));
}

// Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1::
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


RegisterList Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=11 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1 ||
//         inst(15:12)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=11 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  ==
          0x00300000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)(0)=1 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1 ||
//         inst(15:12)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)(0)=1 => UNDEFINED
  if ((((inst.Bits() & 0x00300000) >> 20) & 0x00000001)  ==
          0x00000001)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1::
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


RegisterList Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=~10 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2::
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


RegisterList Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:16)(0)=1 ||
//         inst(3:0)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1::
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


RegisterList Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 ||
//         (inst(8)=1 &&
//         inst(19:16)(0)=1) => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1::
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


RegisterList Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1 ||
//         inst(15:12)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1
//
// Actual:
//   {safety: [inst(11:8)(0)=0 ||
//         inst(11:8)(3:2)=11 => DECODER_ERROR,
//      inst(6)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED]}

SafetyLevel Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1::
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


// Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=~00 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1::
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


RegisterList Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:16)=000xxx => DECODER_ERROR,
//      inst(21:16)=0xxxxx => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21:16)=000xxx => DECODER_ERROR
  if ((inst.Bits() & 0x00380000)  ==
          0x00000000)
    return DECODER_ERROR;

  // inst(21:16)=0xxxxx => UNDEFINED
  if ((inst.Bits() & 0x00200000)  ==
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


RegisterList Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(21)=1 &&
//         inst(19:16)(0)=1 => UNDEFINED,
//      inst(22):inst(5)(1:0)=11 => UNDEFINED],
//    uses: {inst(15:12)}}

RegisterList Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(21)=1 &&
  //       inst(19:16)(0)=1 => UNDEFINED
  if (((inst.Bits() & 0x00200000)  ==
          0x00200000) &&
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001))
    return UNDEFINED;

  // inst(22):inst(5)(1:0)=11 => UNDEFINED
  if (((((((inst.Bits() & 0x00400000) >> 22)) << 1) | ((inst.Bits() & 0x00000020) >> 5)) & 0x00000003)  ==
          0x00000003)
    return UNDEFINED;

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         32  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:8)=0111
//         else 2
//         if inst(11:8)=1010
//         else 3
//         if inst(11:8)=0110
//         else 4
//         if inst(11:8)=0010
//         else 0 => UNPREDICTABLE,
//      inst(11:8)=0110 &&
//         inst(5:4)(1)=1 => UNDEFINED,
//      inst(11:8)=0111 &&
//         inst(5:4)(1)=1 => UNDEFINED,
//      inst(11:8)=1010 &&
//         inst(5:4)=11 => UNDEFINED,
//      not inst(11:8)=0111 ||
//         inst(11:8)=1010 ||
//         inst(11:8)=0110 ||
//         inst(11:8)=0010 => DECODER_ERROR],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1::
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

SafetyLevel Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1::
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


bool Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1::
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

// Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         32  <=
//            inst(22):inst(15:12) + 1
//         if inst(5)=0
//         else 2 => UNPREDICTABLE,
//      inst(7:6)=11 ||
//         (inst(7:6)=00 &&
//         inst(4)=1) => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1::
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

SafetyLevel Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1::
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


bool Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1::
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

// Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) => UNPREDICTABLE,
//      inst(11:10)=00 &&
//         inst(7:4)(0)=~0 => UNDEFINED,
//      inst(11:10)=01 &&
//         inst(7:4)(1)=~0 => UNDEFINED,
//      inst(11:10)=10 &&
//         inst(7:4)(1:0)=~00 &&
//         inst(7:4)(1:0)=~11 => UNDEFINED,
//      inst(11:10)=10 &&
//         inst(7:4)(2)=~0 => UNDEFINED,
//      inst(11:10)=11 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1::
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

SafetyLevel Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1::
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


bool Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1::
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

// Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         32  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:8)=1000
//         else 2 + 1
//         if inst(11:8)=1000 ||
//         inst(11:8)=1001
//         else 2 => UNPREDICTABLE,
//      inst(11:8)=1000 ||
//         inst(11:8)=1001 &&
//         inst(5:4)=11 => UNDEFINED,
//      inst(7:6)=11 => UNDEFINED,
//      not inst(11:8)=1000 ||
//         inst(11:8)=1001 ||
//         inst(11:8)=0011 => DECODER_ERROR],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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

SafetyLevel Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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


bool Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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

// Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(5)=0
//         else 2 => UNPREDICTABLE,
//      inst(7:6)=11 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1::
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

SafetyLevel Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1::
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


bool Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1::
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

// Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:10)=00
//         else (1
//         if inst(7:4)(1)=0
//         else 2)
//         if inst(11:10)=01
//         else (1
//         if inst(7:4)(2)=0
//         else 2)
//         if inst(11:10)=10
//         else 0 => UNPREDICTABLE,
//      inst(11:10)=10 &&
//         inst(7:4)(1)=~0 => UNDEFINED,
//      inst(11:10)=11 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1::
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

SafetyLevel Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1::
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


bool Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1::
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

// Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:8)=0100
//         else 2 + 1
//         if inst(11:8)=0100
//         else 2 => UNPREDICTABLE,
//      inst(7:6)=11 ||
//         inst(5:4)(1)=1 => UNDEFINED,
//      not inst(11:8)=0100 ||
//         inst(11:8)=0101 => DECODER_ERROR],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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

SafetyLevel Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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


bool Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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

// Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(5)=0
//         else 2 + 1
//         if inst(5)=0
//         else 2 => UNPREDICTABLE,
//      inst(7:6)=11 ||
//         inst(4)=1 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1::
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

SafetyLevel Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1::
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


bool Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1::
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

// Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:10)=00
//         else (1
//         if inst(7:4)(1)=0
//         else 2)
//         if inst(11:10)=01
//         else (1
//         if inst(7:4)(2)=0
//         else 2)
//         if inst(11:10)=10
//         else 0 + 1
//         if inst(11:10)=00
//         else (1
//         if inst(7:4)(1)=0
//         else 2)
//         if inst(11:10)=01
//         else (1
//         if inst(7:4)(2)=0
//         else 2)
//         if inst(11:10)=10
//         else 0 => UNPREDICTABLE,
//      inst(11:10)=00 &&
//         inst(7:4)(0)=~0 => UNDEFINED,
//      inst(11:10)=01 &&
//         inst(7:4)(0)=~0 => UNDEFINED,
//      inst(11:10)=10 &&
//         inst(7:4)(1:0)=~00 => UNDEFINED,
//      inst(11:10)=11 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1::
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

SafetyLevel Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1::
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


bool Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1::
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

// Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:8)=0000
//         else 2 + 1
//         if inst(11:8)=0000
//         else 2 + 1
//         if inst(11:8)=0000
//         else 2 => UNPREDICTABLE,
//      inst(7:6)=11 => UNDEFINED,
//      not inst(11:8)=0000 ||
//         inst(11:8)=0001 => DECODER_ERROR],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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

SafetyLevel Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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


bool Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1::
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

// Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(5)=0
//         else 2 + 1
//         if inst(5)=0
//         else 2 + 1
//         if inst(5)=0
//         else 2 => UNPREDICTABLE,
//      inst(7:6)=11 &&
//         inst(4)=0 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1::
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

SafetyLevel Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1::
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


bool Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1::
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

// Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)}
//         if (15  !=
//            inst(3:0))
//         else {},
//    safety: [15  ==
//            inst(19:16) ||
//         31  <=
//            inst(22):inst(15:12) + 1
//         if inst(11:10)=00
//         else (1
//         if inst(7:4)(1)=0
//         else 2)
//         if inst(11:10)=01
//         else (1
//         if inst(7:4)(2)=0
//         else 2)
//         if inst(11:10)=10
//         else 0 + 1
//         if inst(11:10)=00
//         else (1
//         if inst(7:4)(1)=0
//         else 2)
//         if inst(11:10)=01
//         else (1
//         if inst(7:4)(2)=0
//         else 2)
//         if inst(11:10)=10
//         else 0 + 1
//         if inst(11:10)=00
//         else (1
//         if inst(7:4)(1)=0
//         else 2)
//         if inst(11:10)=01
//         else (1
//         if inst(7:4)(2)=0
//         else 2)
//         if inst(11:10)=10
//         else 0 => UNPREDICTABLE,
//      inst(11:10)=10 &&
//         inst(7:4)(1:0)=11 => UNDEFINED,
//      inst(11:10)=11 => UNDEFINED],
//    small_imm_base_wb: not (15  !=
//            inst(3:0) &&
//         13  !=
//            inst(3:0)),
//    uses: {inst(3:0)
//         if (15  !=
//            inst(3:0))
//         else 32, inst(19:16)}}

Register Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1::
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

SafetyLevel Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1::
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


bool Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'not (15  !=
  //          inst(3:0) &&
  //       13  !=
  //          inst(3:0))'
  return !((((((inst.Bits() & 0x0000000F)) != (15))) &&
       ((((inst.Bits() & 0x0000000F)) != (13)))));
}

RegisterList Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1::
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

// Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(21)=1
//         else 32},
//    safety: [0  ==
//            inst(7:0) ||
//         32  <=
//            inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//      15  ==
//            inst(19:16) &&
//         inst(21)=1 => UNPREDICTABLE,
//      inst(23)  ==
//            inst(24) &&
//         inst(21)=1 => UNDEFINED,
//      inst(24)=0 &&
//         inst(23)=0 &&
//         inst(21)=0 => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(23)=1 &&
//         inst(21)=1 &&
//         13  ==
//            inst(19:16) => DECODER_ERROR,
//      inst(24)=1 &&
//         inst(21)=0 => DECODER_ERROR],
//    small_imm_base_wb: inst(21)=1,
//    uses: {inst(19:16)}}

Register Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1::
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

SafetyLevel Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1::
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


bool Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(21)=1
//         else 32},
//    safety: [0  ==
//            inst(7:0) / 2 ||
//         16  <=
//            inst(7:0) / 2 ||
//         32  <=
//            inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//      15  ==
//            inst(19:16) &&
//         inst(21)=1 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         16  <=
//            inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//      inst(23)  ==
//            inst(24) &&
//         inst(21)=1 => UNDEFINED,
//      inst(24)=0 &&
//         inst(23)=0 &&
//         inst(21)=0 => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(23)=1 &&
//         inst(21)=1 &&
//         13  ==
//            inst(19:16) => DECODER_ERROR,
//      inst(24)=1 &&
//         inst(21)=0 => DECODER_ERROR],
//    small_imm_base_wb: inst(21)=1,
//    uses: {inst(19:16)}}

Register Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1::
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

SafetyLevel Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1::
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

  return MAY_BE_SAFE;
}


bool Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {},
//    is_literal_load: 15  ==
//            inst(19:16),
//    uses: {inst(19:16)}}

Register Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

bool Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1::
is_literal_load(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // is_literal_load: '15  ==
  //          inst(19:16)'
  return ((((inst.Bits() & 0x000F0000) >> 16)) == (15));
}

RegisterList Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [(inst(21:20)=00 ||
//         inst(15:12)(0)=1) => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1::
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


RegisterList Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [(inst(21:20)=00 ||
//         inst(21:20)=01) => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR,
//      inst(24)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
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


RegisterList Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=00 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR,
//      inst(24)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2::
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


RegisterList Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 => UNDEFINED,
//      inst(3:0)(0)=1 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1::
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


RegisterList Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(22:21):inst(6:5)(3:0)=0x10 => UNDEFINED],
//    uses: {inst(15:12)}}

RegisterList Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1::
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


RegisterList Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1
//
// Actual:
//   {defs: {inst(15:12)
//         if inst(20)=1
//         else 32},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {inst(15:12)
//         if not inst(20)=1
//         else 32}}

RegisterList Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1::
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

SafetyLevel Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1::
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

// Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12), inst(19:16)}
//         if inst(20)=1
//         else {},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) => UNPREDICTABLE,
//      inst(20)=1 &&
//         inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {}
//         if inst(20)=1
//         else {inst(15:12), inst(19:16)}}

RegisterList Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1::
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

SafetyLevel Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1::
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


RegisterList Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1::
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

// Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12), inst(19:16)}
//         if inst(20)=1
//         else {},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(19:16) ||
//         31  ==
//            inst(3:0):inst(5) => UNPREDICTABLE,
//      inst(20)=1 &&
//         inst(15:12)  ==
//            inst(19:16) => UNPREDICTABLE],
//    uses: {}
//         if inst(20)=1
//         else {inst(15:12), inst(19:16)}}

RegisterList Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1::
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

SafetyLevel Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1::
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


RegisterList Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1::
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

// Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1
//
// Actual:
//   {safety: [inst(5)=0 &&
//         inst(11:8)(0)=1 &&
//         inst(11:8)(3:2)=~11 => DECODER_ERROR,
//      inst(5)=1 &&
//         inst(11:8)=~1110 => DECODER_ERROR,
//      inst(6)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED]}

SafetyLevel Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1::
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


// Actual_VMRS_cccc111011110001tttt101000010000_case_1
//
// Actual:
//   {defs: {16
//         if 15  ==
//            inst(15:12)
//         else inst(15:12)}}

RegisterList Actual_VMRS_cccc111011110001tttt101000010000_case_1::
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

// Actual_VMSR_cccc111011100001tttt101000010000_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {inst(15:12)}}

RegisterList Actual_VMSR_cccc111011100001tttt101000010000_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMSR_cccc111011100001tttt101000010000_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(15:12) => UNPREDICTABLE
  if (((((inst.Bits() & 0x0000F000) >> 12)) == (15)))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


RegisterList Actual_VMSR_cccc111011100001tttt101000010000_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(15:12)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x0000F000) >> 12)));
}

// Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR,
//      inst(24)=1 ||
//         inst(21:20)=~00 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1::
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


RegisterList Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=~00 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1 ||
//         inst(15:12)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001))))
    return UNDEFINED;

  // inst(21:20)=~00 => UNDEFINED
  if ((inst.Bits() & 0x00300000)  !=
          0x00000000)
    return UNDEFINED;

  return MAY_BE_SAFE;
}


RegisterList Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1
//
// Actual:
//   {safety: [(inst(11:8)(0)=1 &&
//         inst(11:8)(3:2)=~11) ||
//         inst(11:8)(3:1)=111 => DECODER_ERROR,
//      inst(6)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED]}

SafetyLevel Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1::
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


// Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)(0)=1 ||
//         inst(6)=1 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1::
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


RegisterList Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=11 => UNDEFINED,
//      inst(6)=1 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1::
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


RegisterList Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1
//
// Actual:
//   {base: 13,
//    defs: {13},
//    safety: [0  ==
//            inst(7:0) ||
//         32  <=
//            inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE],
//    small_imm_base_wb: true,
//    uses: {13}}

Register Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '13'
  return Register(13);
}

RegisterList Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{13}'
  return RegisterList().
   Add(Register(13));
}

SafetyLevel Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1::
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


bool Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{13}'
  return RegisterList().
   Add(Register(13));
}

// Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1
//
// Actual:
//   {base: 13,
//    defs: {13},
//    safety: [0  ==
//            inst(7:0) / 2 ||
//         16  <=
//            inst(7:0) / 2 ||
//         32  <=
//            inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         16  <=
//            inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE],
//    small_imm_base_wb: true,
//    uses: {13}}

Register Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: '13'
  return Register(13);
}

RegisterList Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{13}'
  return RegisterList().
   Add(Register(13));
}

SafetyLevel Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1::
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

  return MAY_BE_SAFE;
}


bool Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'true'
  return true;
}

RegisterList Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{13}'
  return RegisterList().
   Add(Register(13));
}

// Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=00 ||
//         inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1::
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


RegisterList Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [(inst(21:20)=11 ||
//         inst(21:20)=00) => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1 ||
//         inst(15:12)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // inst(6)=1 &&
  //       (inst(15:12)(0)=1 ||
  //       inst(19:16)(0)=1 ||
  //       inst(15:12)(0)=1) => UNDEFINED
  if (((inst.Bits() & 0x00000040)  ==
          0x00000040) &&
       ((((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x000F0000) >> 16) & 0x00000001)  ==
          0x00000001) ||
       ((((inst.Bits() & 0x0000F000) >> 12) & 0x00000001)  ==
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


RegisterList Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1
//
// Actual:
//   {safety: [inst(19:18)=11 ||
//         inst(3:0)(0)=1 => UNDEFINED,
//      inst(7:6)=00 => DECODER_ERROR]}

SafetyLevel Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1::
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


// Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:16)=000xxx => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(8)=0 => DECODER_ERROR,
//      inst(3:0)(0)=1 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1::
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


RegisterList Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(24)=0 &&
//         inst(8)=0 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED,
//      inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1::
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


RegisterList Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [3  <
//            inst(8:7) + inst(19:18) => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1::
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


RegisterList Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:16)=000xxx => DECODER_ERROR,
//      inst(3:0)(0)=1 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1::
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


RegisterList Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED,
//      inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1::
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


RegisterList Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:16)=000xxx => DECODER_ERROR],
//    uses: {}}

RegisterList Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1::
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


RegisterList Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 ||
//         inst(15:12)(0)=1 => UNDEFINED],
//    uses: {}}

RegisterList Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1::
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


RegisterList Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(21)=1
//         else 32},
//    safety: [0  ==
//            inst(7:0) ||
//         32  <=
//            inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE,
//      15  ==
//            inst(19:16) &&
//         inst(21)=1 => UNPREDICTABLE,
//      inst(23)  ==
//            inst(24) &&
//         inst(21)=1 => UNDEFINED,
//      inst(24)=0 &&
//         inst(23)=0 &&
//         inst(21)=0 => DECODER_ERROR,
//      inst(24)=1 &&
//         inst(21)=0 => DECODER_ERROR,
//      inst(24)=1 &&
//         inst(23)=0 &&
//         inst(21)=1 &&
//         13  ==
//            inst(19:16) => DECODER_ERROR],
//    small_imm_base_wb: inst(21)=1,
//    uses: {inst(19:16)}}

Register Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1::
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

SafetyLevel Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1::
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

  // 0  ==
  //          inst(7:0) ||
  //       32  <=
  //          inst(15:12):inst(22) + inst(7:0) => UNPREDICTABLE
  if (((((inst.Bits() & 0x000000FF)) == (0))) ||
       ((((((((inst.Bits() & 0x0000F000) >> 12)) << 1) | ((inst.Bits() & 0x00400000) >> 22)) + (inst.Bits() & 0x000000FF)) > (32))))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}


bool Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {inst(19:16)
//         if inst(21)=1
//         else 32},
//    safety: [0  ==
//            inst(7:0) / 2 ||
//         16  <=
//            inst(7:0) / 2 ||
//         32  <=
//            inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//      15  ==
//            inst(19:16) &&
//         inst(21)=1 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         16  <=
//            inst(22):inst(15:12) + inst(7:0) / 2 => UNPREDICTABLE,
//      inst(23)  ==
//            inst(24) &&
//         inst(21)=1 => UNDEFINED,
//      inst(24)=0 &&
//         inst(23)=0 &&
//         inst(21)=0 => DECODER_ERROR,
//      inst(24)=1 &&
//         inst(21)=0 => DECODER_ERROR,
//      inst(24)=1 &&
//         inst(23)=0 &&
//         inst(21)=1 &&
//         13  ==
//            inst(19:16) => DECODER_ERROR],
//    small_imm_base_wb: inst(21)=1,
//    uses: {inst(19:16)}}

Register Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1::
base_address_register(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // base: 'inst(19:16)'
  return Register(((inst.Bits() & 0x000F0000) >> 16));
}

RegisterList Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1::
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

SafetyLevel Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1::
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

  return MAY_BE_SAFE;
}


bool Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1::
base_address_register_writeback_small_immediate(
      Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // small_imm_base_wb: 'inst(21)=1'
  return (inst.Bits() & 0x00200000)  ==
          0x00200000;
}

RegisterList Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(19:16) => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16)}}

RegisterList Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1::
safety(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.

  // 15  ==
  //          inst(19:16) => FORBIDDEN_OPERANDS
  if (((((inst.Bits() & 0x000F0000) >> 16)) == (15)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


RegisterList Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{inst(19:16)}'
  return RegisterList().
   Add(Register(((inst.Bits() & 0x000F0000) >> 16)));
}

// Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=~00 => UNDEFINED,
//      inst(22):inst(15:12)  ==
//            inst(5):inst(3:0) => UNKNOWN,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1::
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


RegisterList Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 => UNDEFINED,
//      inst(22):inst(15:12)  ==
//            inst(5):inst(3:0) => UNKNOWN,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1::
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


RegisterList Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

// Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 ||
//         (inst(6)=0 &&
//         inst(19:18)=10) => UNDEFINED,
//      inst(22):inst(15:12)  ==
//            inst(5):inst(3:0) => UNKNOWN,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}

RegisterList Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1::
defs(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // defs: '{}'
  return RegisterList();
}

SafetyLevel Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1::
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


RegisterList Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1::
uses(Instruction inst) const {
  UNREFERENCED_PARAMETER(inst);  // To silence compiler.
  // uses: '{}'
  return RegisterList();
}

}  // namespace nacl_arm_dec
