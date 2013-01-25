/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_ACTUALS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_ACTUALS_H_

#include "native_client/src/trusted/validator_arm/inst_classes.h"
#include "native_client/src/trusted/validator_arm/arm_helpers.h"

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010101snnnnddddiiiiiiiiiiii,
//    rule: ADC_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010000snnnnddddiiiiiiiiiiii,
//    rule: AND_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010001snnnnddddiiiiiiiiiiii,
//    rule: EOR_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010011snnnnddddiiiiiiiiiiii,
//    rule: RSB_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010111snnnnddddiiiiiiiiiiii,
//    rule: RSC_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010110snnnnddddiiiiiiiiiiii,
//    rule: SBC_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
class Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ADC_immediate_cccc0010101snnnnddddiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000101snnnnddddiiiiitt0mmmm,
//    rule: ADC_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000100snnnnddddiiiiitt0mmmm,
//    rule: ADD_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000000snnnnddddiiiiitt0mmmm,
//    rule: AND_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001110snnnnddddiiiiitt0mmmm,
//    rule: BIC_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000001snnnnddddiiiiitt0mmmm,
//    rule: EOR_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001100snnnnddddiiiiitt0mmmm,
//    rule: ORR_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000011snnnnddddiiiiitt0mmmm,
//    rule: RSB_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000111snnnnddddiiiiitt0mmmm,
//    rule: RSC_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000110snnnnddddiiiiitt0mmmm,
//    rule: SBC_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//    rule: SUB_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
class Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000101snnnnddddssss0tt1mmmm,
//    rule: ADC_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000100snnnnddddssss0tt1mmmm,
//    rule: ADD_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000000snnnnddddssss0tt1mmmm,
//    rule: AND_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0001110snnnnddddssss0tt1mmmm,
//    rule: BIC_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000001snnnnddddssss0tt1mmmm,
//    rule: EOR_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0001100snnnnddddssss0tt1mmmm,
//    rule: ORR_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000011snnnnddddssss0tt1mmmm,
//    rule: RSB_register_shfited_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000111snnnnddddssss0tt1mmmm,
//    rule: RSC_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000110snnnnddddssss0tt1mmmm,
//    rule: SBC_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Binary4RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0000010snnnnddddssss0tt1mmmm,
//    rule: SUB_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
class Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOpAddSub,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010100snnnnddddiiiiiiiiiiii,
//    rule: ADD_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      (Rn(19:16)=1111 &&
//         S(20)=0) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOpAddSub,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0010010snnnnddddiiiiiiiiiiii,
//    rule: SUB_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      (Rn(19:16)=1111 &&
//         S(20)=0) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
class Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1);
};

// Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {15}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    baseline: Unary1RegisterImmediateOpPc,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc001010001111ddddiiiiiiiiiiii,
//    rule: ADR_A1,
//    safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {Pc}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    baseline: Unary1RegisterImmediateOpPc,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc001001001111ddddiiiiiiiiiiii,
//    rule: ADR_A2,
//    safety: [Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {Pc}}
class Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ADR_A1_cccc001010001111ddddiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001101s0000ddddiiiii100mmmm,
//    rule: ASR_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001101s0000ddddiiiii010mmmm,
//    rule: LSR_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28), S(20), Rd(15:12), Rm(3:0)],
//    pattern: cccc0001101s0000dddd00000000mmmm,
//    rule: MOV_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001111s0000ddddiiiiitt0mmmm,
//    rule: MVN_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28), S(20), Rd(15:12), Rm(3:0)],
//    pattern: cccc0001101s0000dddd00000110mmmm,
//    rule: RRX,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {Rm}}
class Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ASR_immediate_cccc0001101s0000ddddiiiii100mmmm_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc0001101s0000ddddmmmm0101nnnn,
//    rule: ASR_register,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc0001101s0000ddddmmmm0001nnnn,
//    rule: LSL_register,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc0001101s0000ddddmmmm0011nnnn,
//    rule: LSR_register,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rs: Rs(11:8),
//    S: S(20),
//    baseline: Unary3RegisterShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(15:12),
//      Rs(11:8),
//      type(6:5),
//      Rm(3:0)],
//    pattern: cccc0001111s0000ddddssss0tt1mmmm,
//    rule: MVN_register_shifted_register,
//    safety: [Pc in {Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rd(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc0001101s0000ddddmmmm0111nnnn,
//    rule: ROR_register,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm}}
class Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ASR_register_cccc0001101s0000ddddmmmm0101nnnn_case_1);
};

// Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(20:16)  <
//            inst(11:7) => UNPREDICTABLE],
//    uses: {inst(15:12)}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    arch: v6T2,
//    baseline: Unary1RegisterBitRangeMsbGeLsb,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), msb(20:16), Rd(15:12), lsb(11:7)],
//    lsb: lsb(11:7),
//    msb: msb(20:16),
//    pattern: cccc0111110mmmmmddddlllll0011111,
//    rule: BFC,
//    safety: [Rd  ==
//            Pc => UNPREDICTABLE,
//      msb  <
//            lsb => UNPREDICTABLE],
//    uses: {Rd}}
class Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1
     : public ClassDecoder {
 public:
  Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BFC_cccc0111110mmmmmddddlllll0011111_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6T2,
//    baseline: Binary2RegisterBitRangeMsbGeLsb,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), msb(20:16), Rd(15:12), lsb(11:7), Rn(3:0)],
//    lsb: lsb(11:7),
//    msb: msb(20:16),
//    pattern: cccc0111110mmmmmddddlllll001nnnn,
//    rule: BFI,
//    safety: [Rn  ==
//            Pc => DECODER_ERROR,
//      Rd  ==
//            Pc => UNPREDICTABLE,
//      msb  <
//            lsb => UNPREDICTABLE],
//    uses: {Rn, Rd}}
class Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BFI_cccc0111110mmmmmddddlllll001nnnn_case_1);
};

// Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1
//
// Actual:
//   {clears_bits: (ARMExpandImm(inst(11:0)) &&
//         clears_mask())  ==
//            clears_mask(),
//    defs: {inst(15:12), 16
//         if inst(20)=1
//         else 32},
//    safety: [(inst(15:12)=1111 &&
//         inst(20)=1) => DECODER_ERROR,
//      inst(15:12)=1111 => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16)}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: MaskedBinary2RegisterImmediateOp,
//    clears_bits: (imm32 &&
//         clears_mask())  ==
//            clears_mask(),
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0011110snnnnddddiiiiiiiiiiii,
//    rule: BIC_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
class Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual bool clears_bits(Instruction i, uint32_t clears_mask) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BIC_immediate_cccc0011110snnnnddddiiiiiiiiiiii_case_1);
};

// Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1
//
// Actual:
//   {defs: {},
//    pool_head: true,
//    safety: [inst(31:28)=~1110 => UNPREDICTABLE,
//      not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS],
//    uses: {}}
//
// Baseline:
//   {arch: v5T,
//    baseline: BreakPointAndConstantPoolHead,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28), imm12(19:8), imm4(3:0)],
//    imm12: imm12(19:8),
//    imm32: ZeroExtend(imm12:imm4, 32),
//    imm4: imm4(3:0),
//    inst: inst,
//    pattern: cccc00010010iiiiiiiiiiii0111iiii,
//    pool_head: true,
//    rule: BKPT,
//    safety: [cond(31:28)=~1110 => UNPREDICTABLE,
//      not IsBreakPointAndConstantPoolHead(inst) => FORBIDDEN_OPERANDS],
//    true: true,
//    uses: {}}
class Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1
     : public ClassDecoder {
 public:
  Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_literal_pool_head(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BKPT_cccc00010010iiiiiiiiiiii0111iiii_case_1);
};

// Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {},
//    safety: [true => FORBIDDEN],
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 1111101hiiiiiiiiiiiiiiiiiiiiiiii,
//    rule: BLX_immediate,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 11111110iiiiiiiiiiiiiiiiiii0iiii,
//    rule: CDP2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc1110oooonnnnddddccccooo0mmmm,
//    rule: CDP,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v7,
//    baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc001100100000111100001111iiii,
//    rule: DBG,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 1111110pudw1nnnniiiiiiiiiiiiiiii,
//    rule: LDC2_immediate,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 1111110pudw11111iiiiiiiiiiiiiiii,
//    rule: LDC2_literal,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc110pudw1nnnnddddcccciiiiiiii,
//    rule: LDC_immediate,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc110pudw11111ddddcccciiiiiiii,
//    rule: LDC_literal,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 11111110iii0iiiittttiiiiiii1iiii,
//    rule: MCR2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 111111000100ssssttttiiiiiiiiiiii,
//    rule: MCRR2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5TE,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc11000100ttttttttccccoooommmm,
//    rule: MCRR,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc1110ooo0nnnnttttccccooo1mmmm,
//    rule: MCR,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 11111110iii1iiiittttiiiiiii1iiii,
//    rule: MRC2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc1110ooo1nnnnttttccccooo1mmmm,
//    rule: MRC,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 111111000101ssssttttiiiiiiiiiiii,
//    rule: MRRC2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5TE,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc11000101ttttttttccccoooommmm,
//    rule: MRRC,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc00110r10mmmm1111iiiiiiiiiiii,
//    rule: MSR_immediate,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 1111100pu0w1nnnn0000101000000000,
//    rule: RFE,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6K,
//    baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000100,
//    rule: SEV,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 1111100pu1w0110100000101000iiiii,
//    rule: SRS,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: 1111110pudw0nnnniiiiiiiiiiiiiiii,
//    rule: STC2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc110pudw0nnnnddddcccciiiiiiii,
//    rule: STC,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    pattern: cccc1111iiiiiiiiiiiiiiiiiiiiiiii,
//    rule: SVC,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6K,
//    baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000010,
//    rule: WFE,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {arch: v6K,
//    baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000011,
//    rule: WFI,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BLX_immediate_1111101hiiiiiiiiiiiiiiiiiiiiiiii_case_1);
};

// Actual_BLX_register_cccc000100101111111111110011mmmm_case_1
//
// Actual:
//   {defs: {15, 14},
//    safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS],
//    target: inst(3:0),
//    uses: {inst(3:0)}}
//
// Baseline:
//   {Lr: 14,
//    Pc: 15,
//    Rm: Rm(3:0),
//    arch: v5T,
//    baseline: BranchToRegister,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Pc, Lr},
//    fields: [cond(31:28), Rm(3:0)],
//    pattern: cccc000100101111111111110011mmmm,
//    rule: BLX_register,
//    safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//    target: Rm,
//    uses: {Rm}}
class Actual_BLX_register_cccc000100101111111111110011mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_BLX_register_cccc000100101111111111110011mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BLX_register_cccc000100101111111111110011mmmm_case_1);
};

// Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {15, 14},
//    relative: true,
//    relative_offset: SignExtend(inst(23:0):0(1:0), 32),
//    safety: [true => MAY_BE_SAFE],
//    uses: {15}}
//
// Baseline:
//   {Cond: Cond(31:28),
//    Lr: 14,
//    Pc: 15,
//    baseline: BranchImmediate24,
//    constraints: ,
//    defs: {Pc, Lr},
//    fields: [Cond(31:28), imm24(23:0)],
//    imm24: imm24(23:0),
//    imm32: SignExtend(imm24:0(1:0), 32),
//    pattern: cccc1011iiiiiiiiiiiiiiiiiiiiiiii,
//    relative: true,
//    relative_offset: imm32,
//    rule: BL_BLX_immediate,
//    safety: [true => MAY_BE_SAFE],
//    true: true,
//    uses: {Pc}}
class Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_relative_branch(Instruction i) const;
  virtual int32_t branch_target_offset(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_BL_BLX_immediate_cccc1011iiiiiiiiiiiiiiiiiiiiiiii_case_1);
};

// Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {15},
//    relative: true,
//    relative_offset: SignExtend(inst(23:0):0(1:0), 32),
//    safety: [true => MAY_BE_SAFE],
//    uses: {15}}
//
// Baseline:
//   {Cond: Cond(31:28),
//    Pc: 15,
//    baseline: BranchImmediate24,
//    constraints: ,
//    defs: {Pc},
//    fields: [Cond(31:28), imm24(23:0)],
//    imm24: imm24(23:0),
//    imm32: SignExtend(imm24:0(1:0), 32),
//    pattern: cccc1010iiiiiiiiiiiiiiiiiiiiiiii,
//    relative: true,
//    relative_offset: imm32,
//    rule: B,
//    safety: [true => MAY_BE_SAFE],
//    true: true,
//    uses: {Pc}}
class Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_relative_branch(Instruction i) const;
  virtual int32_t branch_target_offset(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_B_cccc1010iiiiiiiiiiiiiiiiiiiiiiii_case_1);
};

// Actual_Bx_cccc000100101111111111110001mmmm_case_1
//
// Actual:
//   {defs: {15},
//    safety: [inst(3:0)=1111 => FORBIDDEN_OPERANDS],
//    target: inst(3:0),
//    uses: {inst(3:0)}}
//
// Baseline:
//   {Pc: 15,
//    Rm: Rm(3:0),
//    arch: v4T,
//    baseline: BranchToRegister,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Pc},
//    fields: [cond(31:28), Rm(3:0)],
//    pattern: cccc000100101111111111110001mmmm,
//    rule: Bx,
//    safety: [Rm(3:0)=1111 => FORBIDDEN_OPERANDS],
//    target: Rm,
//    uses: {Rm}}
class Actual_Bx_cccc000100101111111111110001mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_Bx_cccc000100101111111111110001mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual Register branch_target_register(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_Bx_cccc000100101111111111110001mmmm_case_1);
};

// Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) ||
//         15  ==
//            inst(3:0) => UNPREDICTABLE],
//    uses: {inst(3:0)}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v5T,
//    baseline: Unary2RegisterOpNotRmIsPc,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), Rm(3:0)],
//    pattern: cccc000101101111dddd11110001mmmm,
//    rule: CLZ,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6T2,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), Rm(3:0)],
//    pattern: cccc011011111111dddd11110011mmmm,
//    rule: RBIT,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), Rm(3:0)],
//    pattern: cccc011010111111dddd11111011mmmm,
//    rule: REV16,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), Rm(3:0)],
//    pattern: cccc011011111111dddd11111011mmmm,
//    rule: REVSH,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), Rm(3:0)],
//    pattern: cccc011010111111dddd11110011mmmm,
//    rule: REV,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Unary2RegisterSatImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), sat_imm(19:16), Rd(15:12), Rn(3:0)],
//    pattern: cccc01101010iiiidddd11110011nnnn,
//    rule: SSAT16,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(19:16),
//    saturate_to: sat_imm + 1,
//    uses: {Rn}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Unary2RegisterSatImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28),
//      sat_imm(20:16),
//      Rd(15:12),
//      imm5(11:7),
//      sh(6),
//      Rn(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0110101iiiiiddddiiiiis01nnnn,
//    rule: SSAT,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(20:16),
//    saturate_to: sat_imm + 1,
//    sh: sh(6),
//    shift: DecodeImmShift(sh:0(0), imm5),
//    uses: {Rn}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc011010001111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: SXTB16,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc011010101111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: SXTB,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc011010111111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: SXTH,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Unary2RegisterSatImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), sat_imm(19:16), Rd(15:12), Rn(3:0)],
//    pattern: cccc01101110iiiidddd11110011nnnn,
//    rule: USAT16,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(19:16),
//    saturate_to: sat_imm,
//    uses: {Rn}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Unary2RegisterSatImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28),
//      sat_imm(20:16),
//      Rd(15:12),
//      imm5(11:7),
//      sh(6),
//      Rn(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0110111iiiiiddddiiiiis01nnnn,
//    rule: USAT,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(20:16),
//    saturate_to: sat_imm,
//    sh: sh(6),
//    shift: DecodeImmShift(sh:0(0), imm5),
//    uses: {Rn}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc011011001111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: UXTB16,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc011011101111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: UXTB,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc011011111111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: UXTH,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_CLZ_cccc000101101111dddd11110001mmmm_case_1);
};

// Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {16},
//    uses: {inst(19:16)}}
//
// Baseline:
//   {NZCV: 16,
//    Rn: Rn(19:16),
//    baseline: BinaryRegisterImmediateTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm_C(imm12),
//    pattern: cccc00110111nnnn0000iiiiiiiiiiii,
//    rule: CMN_immediate,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    Rn: Rn(19:16),
//    baseline: BinaryRegisterImmediateTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm_C(imm12),
//    pattern: cccc00110101nnnn0000iiiiiiiiiiii,
//    rule: CMP_immediate,
//    uses: {Rn}}
//
// Baseline:
//   {NZCV: 16,
//    Rn: Rn(19:16),
//    baseline: BinaryRegisterImmediateTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm_C(imm12),
//    pattern: cccc00110011nnnn0000iiiiiiiiiiii,
//    rule: TEQ_immediate,
//    uses: {Rn}}
class Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1);
};

// Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1
//
// Actual:
//   {defs: {16
//         if inst(20)
//         else 32},
//    uses: {inst(19:16), inst(3:0)}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmedShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc00010111nnnn0000iiiiitt0mmmm,
//    rule: CMN_register,
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmedShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc00010101nnnn0000iiiiitt0mmmm,
//    rule: CMP_register,
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmedShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//    rule: TEQ_register,
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmedShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rn(19:16),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//    rule: TST_register,
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
class Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    baseline: Binary3RegisterShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), Rs(11:8), type(6:5), Rm(3:0)],
//    pattern: cccc00010111nnnn0000ssss0tt1mmmm,
//    rule: CMN_register_shifted_register,
//    safety: [Pc in {Rn, Rm, Rs} => UNPREDICTABLE],
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    baseline: Binary3RegisterShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), Rs(11:8), type(6:5), Rm(3:0)],
//    pattern: cccc00010101nnnn0000ssss0tt1mmmm,
//    rule: CMP_register_shifted_register,
//    safety: [Pc in {Rn, Rm, Rs} => UNPREDICTABLE],
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    baseline: Binary3RegisterShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), Rs(11:8), type(6:5), Rm(3:0)],
//    pattern: cccc00010011nnnn0000ssss0tt1mmmm,
//    rule: TEQ_register_shifted_register,
//    safety: [Pc in {Rn, Rm, Rs} => UNPREDICTABLE],
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
//
// Baseline:
//   {NZCV: 16,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    baseline: Binary3RegisterShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), Rs(11:8), type(6:5), Rm(3:0)],
//    pattern: cccc00010001nnnn0000ssss0tt1mmmm,
//    rule: TST_register_shifted_register,
//    safety: [Pc in {Rn, Rm, Rs} => UNPREDICTABLE],
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
class Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1);
};

// Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1
//
// Actual:
//   {safety: [inst(19:18)=~01 => UNDEFINED,
//      inst(8)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED,
//      not inst(8)=1 &&
//         inst(3:0)(0)=1 => UNDEFINED]}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    arch: ASIMDhp,
//    baseline: Vector2RegisterMiscellaneous_CVT_H2S,
//    constraints: ,
//    d: D:Vd,
//    elements: 4,
//    esize: 16,
//    fields: [D(22), size(19:18), Vd(15:12), op(8), M(5), Vm(3:0)],
//    half_to_single: op(8)=1,
//    m: M:Vm,
//    op: op(8),
//    pattern: 111100111d11ss10dddd011p00m0mmmm,
//    rule: CVT_between_half_precision_and_single_precision,
//    safety: [size(19:18)=~01 => UNDEFINED,
//      half_to_single &&
//         Vd(0)=1 => UNDEFINED,
//      not half_to_single &&
//         Vm(0)=1 => UNDEFINED],
//    size: size(19:18)}
class Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1()
     : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_CVT_between_half_precision_and_single_precision_111100111d11ss10dddd011p00m0mmmm_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: LoadRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: Union({Rn
//         if wback
//         else None}, registers),
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100000w1nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: LDMDA_LDMFA,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) => UNKNOWN,
//      Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: LoadRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: Union({Rn
//         if wback
//         else None}, registers),
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100100w1nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: LDMDB_LDMEA,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) => UNKNOWN,
//      Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: LoadRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: Union({Rn
//         if wback
//         else None}, registers),
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100110w1nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: LDMIB_LDMED,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) => UNKNOWN,
//      Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: LoadRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: Union({Rn
//         if wback
//         else None}, registers),
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100010w1nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: LDM_LDMIA_LDMFD,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) => UNKNOWN,
//      Contains(registers, Pc) => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
class Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1
     : public ClassDecoder {
 public:
  Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDMDA_LDMFA_cccc100000w1nnnnrrrrrrrrrrrrrrrr_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Load2RegisterImm12Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ZeroExtend(imm12, 32),
//    index: P(24)=1,
//    pattern: cccc010pu1w1nnnnttttiiiiiiiiiiii,
//    rule: LDRB_immediate,
//    safety: [Rn  ==
//            Pc => DECODER_ERROR,
//      P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rt  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         Rn  ==
//            Rt => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRB_immediate_cccc010pu1w1nnnnttttiiiiiiiiiiii_case_1);
};

// Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: 15,
//    defs: {inst(15:12)},
//    is_literal_load: true,
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {15}}
//
// Baseline:
//   {Pc: 15,
//    Rt: Rt(15:12),
//    U: U(23),
//    add: U(23)=1,
//    base: Pc,
//    baseline: Load2RegisterImm12Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28), U(23), Rt(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ZeroExtend(imm12, 32),
//    is_literal_load: true,
//    pattern: cccc0101u1011111ttttiiiiiiiiiiii,
//    rule: LDRB_literal,
//    safety: [Rt  ==
//            Pc => UNPREDICTABLE],
//    true: true,
//    uses: {Pc}}
class Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRB_literal_cccc0101u1011111ttttiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Load3RegisterImm5Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    index: P(24)=1,
//    pattern: cccc011pu1w1nnnnttttiiiiitt0mmmm,
//    rule: LDRB_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Pc in {Rt, Rm} => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rn  ==
//            Rm => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm, Rn},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRB_register_cccc011pu1w1nnnnttttiiiiitt0mmmm_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    address: offset_addr
//         if index
//         else Rn,
//    arch: v5TE,
//    base: Rn,
//    baseline: Load2RegisterImm8DoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    index: P(24)=1,
//    offset_addr: Rn + imm32
//         if add
//         else Rn - imm32,
//    pattern: cccc000pu1w0nnnnttttiiii1101iiii,
//    rule: LDRD_immediate,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Rt(0)=1 => UNPREDICTABLE,
//      P(24)=0 &&
//         W(21)=1 => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Rt ||
//         Rn  ==
//            Rt2) => UNPREDICTABLE,
//      Rt2  ==
//            Pc => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRD_immediate_cccc000pu1w0nnnnttttiiii1101iiii_case_1);
};

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
//
// Baseline:
//   {P: P(24),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    arch: v5TE,
//    base: Pc,
//    baseline: LoadRegisterImm8DoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    is_literal_load: true,
//    pattern: cccc0001u1001111ttttiiii1101iiii,
//    rule: LDRD_literal,
//    safety: [Rt(0)=1 => UNPREDICTABLE,
//      Rt2  ==
//            Pc => UNPREDICTABLE],
//    true: true,
//    uses: {Pc}}
class Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRD_literal_cccc0001u1001111ttttiiii1101iiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    arch: v5TE,
//    base: Rn,
//    baseline: Load3RegisterDoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      Rm(3:0)],
//    index: P(24)=1,
//    pattern: cccc000pu0w0nnnntttt00001101mmmm,
//    rule: LDRD_register,
//    safety: [Rt(0)=1 => UNPREDICTABLE,
//      P(24)=0 &&
//         W(21)=1 => UNPREDICTABLE,
//      Rt2  ==
//            Pc ||
//         Rm  ==
//            Pc ||
//         Rm  ==
//            Rt ||
//         Rm  ==
//            Rt2 => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt ||
//         Rn  ==
//            Rt2) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rm  ==
//            Rn => UNPREDICTABLE,
//      index => FORBIDDEN],
//    uses: {Rn, Rm},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRD_register_cccc000pu0w0nnnntttt00001101mmmm_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    arch: v6K,
//    base: Rn,
//    baseline: LoadExclusive2RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28), Rn(19:16), Rt(15:12)],
//    imm32: Zeros((32)),
//    pattern: cccc00011101nnnntttt111110011111,
//    rule: LDREXB,
//    safety: [Pc in {Rt, Rn} => UNPREDICTABLE],
//    uses: {Rn}}
//
// Baseline:
//   {Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    arch: v6,
//    base: Rn,
//    baseline: LoadExclusive2RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28), Rn(19:16), Rt(15:12)],
//    imm32: Zeros((32)),
//    pattern: cccc00011001nnnntttt111110011111,
//    rule: LDREX,
//    safety: [Pc in {Rt, Rn} => UNPREDICTABLE],
//    uses: {Rn}}
//
// Baseline:
//   {Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    arch: v6K,
//    base: Rn,
//    baseline: LoadExclusive2RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28), Rn(19:16), Rt(15:12)],
//    imm32: Zeros((32)),
//    pattern: cccc00011111nnnntttt111110011111,
//    rule: STREXH,
//    safety: [Pc in {Rt, Rn} => UNPREDICTABLE],
//    uses: {Rn}}
class Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1
     : public ClassDecoder {
 public:
  Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDREXB_cccc00011101nnnntttt111110011111_case_1);
};

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
//
// Baseline:
//   {Lr: 14,
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    arch: v6K,
//    base: Rn,
//    baseline: LoadExclusive2RegisterDoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2},
//    fields: [cond(31:28), Rn(19:16), Rt(15:12)],
//    imm32: Zeros((32)),
//    pattern: cccc00011011nnnntttt111110011111,
//    rule: LDREXD,
//    safety: [Rt(0)=1 ||
//         Rt  ==
//            Lr ||
//         Rn  ==
//            Pc => UNPREDICTABLE],
//    uses: {Rn}}
class Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1
     : public ClassDecoder {
 public:
  Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDREXD_cccc00011011nnnntttt111110011111_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    address: offset_addr
//         if index
//         else Rn,
//    base: Rn,
//    baseline: Load2RegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    index: P(24)=1,
//    offset_addr: Rn + imm32
//         if add
//         else Rn - imm32,
//    pattern: cccc000pu1w1nnnnttttiiii1011iiii,
//    rule: LDRH_immediate,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rt  ==
//            Pc ||
//         (wback &&
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      Rt  ==
//            Pc => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    address: offset_addr
//         if index
//         else Rn,
//    base: Rn,
//    baseline: Load2RegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    index: P(24)=1,
//    offset_addr: Rn + imm32
//         if add
//         else Rn - imm32,
//    pattern: cccc000pu1w1nnnnttttiiii1101iiii,
//    rule: LDRSB_immediate,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rt  ==
//            Pc ||
//         (wback &&
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      Rt  ==
//            Pc => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    address: offset_addr
//         if index
//         else Rn,
//    base: Rn,
//    baseline: Load2RegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    index: P(24)=1,
//    offset_addr: Rn + imm32
//         if add
//         else Rn - imm32,
//    pattern: cccc000pu1w1nnnnttttiiii1111iiii,
//    rule: LDRSH_immediate,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rt  ==
//            Pc ||
//         (wback &&
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      Rt  ==
//            Pc => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRH_immediate_cccc000pu1w1nnnnttttiiii1011iiii_case_1);
};

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
//
// Baseline:
//   {P: P(24),
//    Pc: 15,
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Pc,
//    baseline: LoadRegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    is_literal_load: true,
//    pattern: cccc000pu1w11111ttttiiii1011iiii,
//    rule: LDRH_literal,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      P  ==
//            W => UNPREDICTABLE,
//      Rt  ==
//            Pc => UNPREDICTABLE],
//    true: true,
//    uses: {Pc}}
//
// Baseline:
//   {P: P(24),
//    Pc: 15,
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Pc,
//    baseline: LoadRegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    is_literal_load: true,
//    pattern: cccc0001u1011111ttttiiii1101iiii,
//    rule: LDRSB_literal,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      P  ==
//            W => UNPREDICTABLE,
//      Rt  ==
//            Pc => UNPREDICTABLE],
//    true: true,
//    uses: {Pc}}
//
// Baseline:
//   {P: P(24),
//    Pc: 15,
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Pc,
//    baseline: LoadRegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    is_literal_load: true,
//    pattern: cccc0001u1011111ttttiiii1111iiii,
//    rule: LDRSB_literal,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      P  ==
//            W => UNPREDICTABLE,
//      Rt  ==
//            Pc => UNPREDICTABLE],
//    true: true,
//    uses: {Pc}}
class Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRH_literal_cccc000pu1w11111ttttiiii1011iiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Load3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      Rm(3:0)],
//    index: P(24)=1,
//    pattern: cccc000pu0w1nnnntttt00001011mmmm,
//    rule: LDRH_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Pc in {Rt, Rm} => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rm  ==
//            Rn => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift_n: 0,
//    shift_t: SRType_LSL(),
//    uses: {Rn, Rm},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Load3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      Rm(3:0)],
//    index: P(24)=1,
//    pattern: cccc000pu0w1nnnntttt00001101mmmm,
//    rule: LDRSB_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Pc in {Rt, Rm} => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rm  ==
//            Rn => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift_n: 0,
//    shift_t: SRType_LSL(),
//    uses: {Rn, Rm},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Load3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      Rm(3:0)],
//    index: P(24)=1,
//    pattern: cccc000pu0w1nnnntttt00001111mmmm,
//    rule: LDRSH_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Pc in {Rt, Rm} => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rm  ==
//            Rn => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift_n: 0,
//    shift_t: SRType_LSL(),
//    uses: {Rn, Rm},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDRH_register_cccc000pu0w1nnnntttt00001011mmmm_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Tp: 9,
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: LdrImmediateOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ZeroExtend(imm12, 32),
//    index: P(24)=1,
//    is_load_tp: Rn  ==
//            Tp &&
//         index &&
//         not wback &&
//         add &&
//         imm12 in {0, 4},
//    pattern: cccc010pu0w1nnnnttttiiiiiiiiiiii,
//    rule: LDR_immediate,
//    safety: [Rn  ==
//            Pc => DECODER_ERROR,
//      P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      wback &&
//         Rn  ==
//            Rt => UNPREDICTABLE,
//      Rt  ==
//            Pc => FORBIDDEN_OPERANDS],
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_load_thread_address_pointer(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDR_immediate_cccc010pu0w1nnnnttttiiiiiiiiiiii_case_1);
};

// Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1
//
// Actual:
//   {base: 15,
//    defs: {inst(15:12)},
//    is_literal_load: true,
//    uses: {15}}
//
// Baseline:
//   {Pc: 15,
//    Rt: Rt(15:12),
//    U: U(23),
//    add: U(23)=1,
//    base: Pc,
//    baseline: Load2RegisterImm12Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    fields: [cond(31:28), U(23), Rt(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ZeroExtend(imm12, 32),
//    is_literal_load: true,
//    pattern: cccc0101u0011111ttttiiiiiiiiiiii,
//    rule: LDR_literal,
//    true: true,
//    uses: {Pc}}
class Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDR_literal_cccc0101u0011111ttttiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Load3RegisterImm5Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    index: P(24)=1,
//    pattern: cccc011pu0w1nnnnttttiiiiitt0mmmm,
//    rule: LDR_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rm  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rn  ==
//            Rm => UNPREDICTABLE,
//      index => FORBIDDEN,
//      Rt  ==
//            Pc => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm, Rn},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LDR_register_cccc011pu0w1nnnnttttiiiiitt0mmmm_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterShiftedOpImmNotZero,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001101s0000ddddiiiii000mmmm,
//    rule: LSL_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      imm5(11:7)=00000 => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    S: S(20),
//    baseline: Unary2RegisterShiftedOpImmNotZero,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc0001101s0000ddddiiiii110mmmm,
//    rule: ROR_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      imm5(11:7)=00000 => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm}}
class Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_LSL_immediate_cccc0001101s0000ddddiiiii000mmmm_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary4RegisterDualOpLtV6RdNotRn,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      Rd(19:16),
//      Ra(15:12),
//      Rm(11:8),
//      Rn(3:0)],
//    pattern: cccc0000001sddddaaaammmm1001nnnn,
//    rule: MLA_A1,
//    safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE,
//      (ArchVersion()  <
//            6 &&
//         Rd  ==
//            Rn) => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm, Ra}}
class Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MLA_A1_cccc0000001sddddaaaammmm1001nnnn_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6T2,
//    baseline: Binary4RegisterDualOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc00000110ddddaaaammmm1001nnnn,
//    rule: MLS_A1,
//    safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
//
// Baseline:
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v5TE,
//    baseline: Binary4RegisterDualOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28),
//      Rd(19:16),
//      Ra(15:12),
//      Rm(11:8),
//      M(6),
//      N(5),
//      Rn(3:0)],
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010000ddddaaaammmm1xx0nnnn,
//    rule: SMLABB_SMLABT_SMLATB_SMLATT,
//    safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
//
// Baseline:
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v5TE,
//    baseline: Binary4RegisterDualOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28),
//      Rd(19:16),
//      Ra(15:12),
//      Rm(11:8),
//      M(6),
//      N(5),
//      Rn(3:0)],
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010010ddddaaaammmm1x00nnnn,
//    rule: SMLAWB_SMLAWT,
//    safety: [Pc in {Rd, Rn, Rm, Ra} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
class Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MLS_A1_cccc00000110ddddaaaammmm1001nnnn_case_1);
};

// Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(23):inst(22:21):inst(6:5)(4:0)=10x00 ||
//         inst(23):inst(22:21):inst(6:5)(4:0)=x0x10 => UNDEFINED]}
//
// Baseline:
//   {N: N(7),
//    Pc: 15,
//    Rt: Rt(15:12),
//    U: U(23),
//    Vn: Vn(19:16),
//    advsimd: sel in bitset {'x1xxx', 'x0xx1'},
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: MoveVfpRegisterOpWithTypeSel,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt},
//    esize: 8
//         if U:opc1:opc2(4:0)=x1xxx
//         else 16
//         if U:opc1:opc2(4:0)=x0xx1
//         else 32
//         if U:opc1:opc2(4:0)=00x00
//         else 0,
//    fields: [cond(31:28),
//      U(23),
//      opc1(22:21),
//      Vn(19:16),
//      Rt(15:12),
//      N(7),
//      opc2(6:5)],
//    index: opc1(0):opc2
//         if U:opc1:opc2(4:0)=x1xxx
//         else opc1(0):opc2(1)
//         if U:opc1:opc2(4:0)=x0xx1
//         else opc1(0)
//         if U:opc1:opc2(4:0)=00x00
//         else 0,
//    n: N:Vn,
//    opc1: opc1(22:21),
//    opc2: opc2(6:5),
//    pattern: cccc1110iii1nnnntttt1011nii10000,
//    rule: MOVE_scalar_to_ARM_core_register,
//    safety: [sel in bitset {'10x00', 'x0x10'} => UNDEFINED,
//      t  ==
//            Pc => UNPREDICTABLE],
//    sel: U:opc1:opc2,
//    t: Rt,
//    unsigned: U(23)=1}
class Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1
     : public ClassDecoder {
 public:
  Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MOVE_scalar_to_ARM_core_register_cccc1110iii1nnnntttt1011nii10000_case_1);
};

// Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {inst(15:12), 16
//         if inst(20)
//         else 32},
//    dynamic_code_replace_immediates: {inst(19:16), inst(11:0)},
//    safety: [inst(15:12)=1111 => UNPREDICTABLE],
//    uses: {}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    S: S(20),
//    arch: v6T2,
//    baseline: Unary1RegisterImmediateOpDynCodeReplace,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    dynamic_code_replace_immediates: {imm4, imm12},
//    fields: [cond(31:28), S(20), imm4(19:16), Rd(15:12), imm12(11:0)],
//    imm: imm4:imm12,
//    imm12: imm12(11:0),
//    imm4: imm4(19:16),
//    pattern: cccc00110100iiiiddddiiiiiiiiiiii,
//    rule: MOVT,
//    safety: [Rd(15:12)=1111 => UNPREDICTABLE],
//    uses: {}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    S: S(20),
//    arch: v6T2,
//    baseline: Unary1RegisterImmediateOpDynCodeReplace,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if S
//         else None},
//    dynamic_code_replace_immediates: {imm4, imm12},
//    fields: [cond(31:28), S(20), imm4(19:16), Rd(15:12), imm12(11:0)],
//    imm: imm4:imm12,
//    imm12: imm12(11:0),
//    imm4: imm4(19:16),
//    pattern: cccc00110000iiiiddddiiiiiiiiiiii,
//    rule: MOVW,
//    safety: [Rd(15:12)=1111 => UNPREDICTABLE],
//    uses: {}}
class Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual Instruction dynamic_code_replacement_sentinel(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MOVT_cccc00110100iiiiddddiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    S: S(20),
//    baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    dynamic_code_replace_immediates: {imm12},
//    fields: [cond(31:28), S(20), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0011101s0000ddddiiiiiiiiiiii,
//    rule: MOV_immediate_A1,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    S: S(20),
//    baseline: Unary1RegisterImmediateOp12DynCodeReplace,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    dynamic_code_replace_immediates: {imm12},
//    fields: [cond(31:28), S(20), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0011111s0000ddddiiiiiiiiiiii,
//    rule: MVN_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {}}
class Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual Instruction dynamic_code_replacement_sentinel(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MOV_immediate_A1_cccc0011101s0000ddddiiiiiiiiiiii_case_1);
};

// Actual_MRS_cccc00010r001111dddd000000000000_case_1
//
// Actual:
//   {defs: {inst(15:12)},
//    safety: [inst(15:12)=1111 => UNPREDICTABLE,
//      inst(22)=1 => FORBIDDEN_OPERANDS],
//    uses: {}}
//
// Baseline:
//   {R: R(22),
//    Rd: Rd(15:12),
//    baseline: Unary1RegisterSet,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), R(22), Rd(15:12)],
//    pattern: cccc00010r001111dddd000000000000,
//    read_spsr: R(22)=1,
//    rule: MRS,
//    safety: [R(22)=1 => FORBIDDEN_OPERANDS,
//      Rd(15:12)=1111 => UNPREDICTABLE],
//    uses: {}}
class Actual_MRS_cccc00010r001111dddd000000000000_case_1
     : public ClassDecoder {
 public:
  Actual_MRS_cccc00010r001111dddd000000000000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MRS_cccc00010r001111dddd000000000000_case_1);
};

// Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {16
//         if inst(19:18)(1)=1
//         else 32},
//    safety: [inst(19:18)=00 => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    baseline: MoveImmediate12ToApsr,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if write_nzcvq
//         else None},
//    fields: [cond(31:28), mask(19:18), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    mask: mask(19:18),
//    pattern: cccc00110010mm001111iiiiiiiiiiii,
//    rule: MSR_immediate,
//    safety: [mask(19:18)=00 => DECODER_ERROR],
//    uses: {},
//    write_g: mask(0)=1,
//    write_nzcvq: mask(1)=1}
class Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MSR_immediate_cccc00110010mm001111iiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rn: Rn(3:0),
//    baseline: Unary1RegisterUse,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if write_nzcvq
//         else None},
//    fields: [cond(31:28), mask(19:18), Rn(3:0)],
//    mask: mask(19:18),
//    pattern: cccc00010010mm00111100000000nnnn,
//    rule: MSR_register,
//    safety: [mask(19:18)=00 => UNPREDICTABLE,
//      Rn  ==
//            Pc => UNPREDICTABLE],
//    uses: {Rn},
//    write_g: mask(0)=1,
//    write_nzcvq: mask(1)=1}
class Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MSR_register_cccc00010010mm00111100000000nnnn_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary3RegisterOpAltA,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rd(19:16), Rm(11:8), Rn(3:0)],
//    pattern: cccc0000000sdddd0000mmmm1001nnnn,
//    rule: MUL_A1,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE,
//      (ArchVersion()  <
//            6 &&
//         Rd  ==
//            Rn) => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rm, Rn}}
class Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_MUL_A1_cccc0000000sdddd0000mmmm1001nnnn_case_1);
};

// Actual_NOP_cccc0011001000001111000000000000_case_1
//
// Actual:
//   {defs: {},
//    uses: {}}
//
// Baseline:
//   {arch: ['v6K', 'v6T2'],
//    baseline: CondDecoder,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000000,
//    rule: NOP,
//    uses: {}}
//
// Baseline:
//   {arch: v6K,
//    baseline: CondDecoder,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    pattern: cccc0011001000001111000000000001,
//    rule: YIELD,
//    uses: {}}
class Actual_NOP_cccc0011001000001111000000000000_case_1
     : public ClassDecoder {
 public:
  Actual_NOP_cccc0011001000001111000000000000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_NOP_cccc0011001000001111000000000000_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    baseline: Binary2RegisterImmediateOpDynCodeReplace,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    dynamic_code_replace_immediates: {imm12},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm(imm12),
//    pattern: cccc0011100snnnnddddiiiiiiiiiiii,
//    rule: ORR_immediate,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    setflags: S(20)=1,
//    uses: {Rn}}
class Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual Instruction dynamic_code_replacement_sentinel(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_ORR_immediate_cccc0011100snnnnddddiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28),
//      Rn(19:16),
//      Rd(15:12),
//      imm5(11:7),
//      tb(6),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    pattern: cccc01101000nnnnddddiiiiit01mmmm,
//    rule: PKH,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    shift: DecodeImmShift(tb:0(0), imm5),
//    tb: tb(6),
//    tbform: tb(6)=1,
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100010nnnndddd11110001mmmm,
//    rule: QADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100010nnnndddd11111001mmmm,
//    rule: QADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Cond: Cond(31:28),
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    constraints: ,
//    defs: {Rd},
//    fields: [Cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc00010000nnnndddd00000101mmmm,
//    rule: QADD,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100010nnnndddd11110011mmmm,
//    rule: QASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Cond: Cond(31:28),
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    constraints: ,
//    defs: {Rd},
//    fields: [Cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc00010100nnnndddd00000101mmmm,
//    rule: QDADD,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Cond: Cond(31:28),
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    constraints: ,
//    defs: {Rd},
//    fields: [Cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc00010110nnnndddd00000101mmmm,
//    rule: QDSUB,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100010nnnndddd11110101mmmm,
//    rule: QSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100010nnnndddd11110111mmmm,
//    rule: QSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100010nnnndddd11111111mmmm,
//    rule: QSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Cond: Cond(31:28),
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    constraints: ,
//    defs: {Rd},
//    fields: [Cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc00010010nnnndddd00000101mmmm,
//    rule: QSUB,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100001nnnndddd11110001mmmm,
//    rule: SADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100001nnnndddd11111001mmmm,
//    rule: SADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100001nnnndddd11110011mmmm,
//    rule: SASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01101000nnnndddd11111011mmmm,
//    rule: SEL,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100011nnnndddd11110001mmmm,
//    rule: SHADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100011nnnndddd11111001mmmm,
//    rule: SHADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100011nnnndddd11110011mmmm,
//    rule: SHASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100011nnnndddd11110101mmmm,
//    rule: SHSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100011nnnndddd11110111mmmm,
//    rule: SHSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100011nnnndddd11111111mmmm,
//    rule: SHSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100001nnnndddd11110101mmmm,
//    rule: SSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100001nnnndddd11110111mmmm,
//    rule: SSSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100001nnnndddd11111111mmmm,
//    rule: SSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100101nnnndddd11110001mmmm,
//    rule: UADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100101nnnndddd11111001mmmm,
//    rule: UADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100101nnnndddd11110011mmmm,
//    rule: UASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100111nnnndddd11110001mmmm,
//    rule: UHADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100111nnnndddd11111001mmmm,
//    rule: UHADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100111nnnndddd11110011mmmm,
//    rule: UHASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100111nnnndddd11110101mmmm,
//    rule: UHSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100111nnnndddd11110111mmmm,
//    rule: UHSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100111nnnndddd11111111mmmm,
//    rule: UHSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100110nnnndddd11110001mmmm,
//    rule: UQADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100110nnnndddd11111001mmmm,
//    rule: UQADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100110nnnndddd11110011mmmm,
//    rule: UQASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100110nnnndddd11110101mmmm,
//    rule: UQSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100110nnnndddd11110111mmmm,
//    rule: UQSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100110nnnndddd11111111mmmm,
//    rule: UQSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100101nnnndddd11110101mmmm,
//    rule: USAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100101nnnndddd11110111mmmm,
//    rule: USUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rm(3:0)],
//    pattern: cccc01100101nnnndddd11111111mmmm,
//    rule: USUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_PKH_cccc01101000nnnnddddiiiiit01mmmm_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6T2,
//    baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), widthm1(20:16), Rd(15:12), lsb(11:7), Rn(3:0)],
//    lsb: lsb(11:7),
//    pattern: cccc0111101wwwwwddddlllll101nnnn,
//    rule: SBFX,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE,
//      lsb + widthm1  >
//            31 => UNPREDICTABLE],
//    uses: {Rn},
//    widthm1: widthm1(20:16)}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6T2,
//    baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), widthm1(20:16), Rd(15:12), lsb(11:7), Rn(3:0)],
//    lsb: lsb(11:7),
//    pattern: cccc0111111mmmmmddddlllll101nnnn,
//    rule: UBFX,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE,
//      lsb + widthm1  >
//            31 => UNPREDICTABLE],
//    uses: {Rn},
//    widthm1: widthm1(20:16)}
class Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SBFX_cccc0111101wwwwwddddlllll101nnnn_case_1);
};

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
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v7VEoptv7A_v7R,
//    baseline: Binary3RegisterOpAltANoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110001dddd1111mmmm0001nnnn,
//    rule: SDIV,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary3RegisterOpAltANoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110101dddd1111mmmm00r1nnnn,
//    rule: SMMUL,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary3RegisterOpAltANoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110000dddd1111mmmm00m1nnnn,
//    rule: SMUAD,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary3RegisterOpAltANoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110000dddd1111mmmm01m1nnnn,
//    rule: SMUSD,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v7VEoptv7A_v7R,
//    baseline: Binary3RegisterOpAltANoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110011dddd1111mmmm0001nnnn,
//    rule: UDIV,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
class Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SDIV_cccc01110001dddd1111mmmm0001nnnn_case_1);
};

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
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualOpNoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Ra(15:12), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110000ddddaaaammmm00m1nnnn,
//    rule: SMLAD,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualOpNoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Ra(15:12), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110000ddddaaaammmm01m1nnnn,
//    rule: SMLSD,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualOpNoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Ra(15:12), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110101ddddaaaammmm00r1nnnn,
//    rule: SMMLA,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualOpNoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Ra(15:12), Rm(11:8), M(5), Rn(3:0)],
//    pattern: cccc01110101ddddaaaammmm11r1nnnn,
//    rule: SMMLS,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
//
// Baseline:
//   {Pc: 15,
//    Ra: Ra(15:12),
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Ra(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc01111000ddddaaaammmm0001nnnn,
//    rule: USADA8,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
class Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SMLAD_cccc01110000ddddaaaammmm00m1nnnn_case_1);
};

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
//
// Baseline:
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v5TE,
//    baseline: Binary4RegisterDualResult,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi},
//    fields: [cond(31:28),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      M(6),
//      N(5),
//      Rn(3:0)],
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010100hhhhllllmmmm1xx0nnnn,
//    rule: SMLALBB_SMLALBT_SMLALTB_SMLALTT,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE],
//    uses: {RdLo, RdHi, Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    baseline: Binary4RegisterDualResult,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi},
//    fields: [cond(31:28), RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//    pattern: cccc00000100hhhhllllmmmm1001nnnn,
//    rule: UMAAL_A1,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE],
//    uses: {RdLo, RdHi, Rn, Rm}}
class Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1);
};

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
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualResultNoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdHi, RdLo},
//    fields: [cond(31:28),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      M(5),
//      Rn(3:0)],
//    pattern: cccc01110100hhhhllllmmmm00m1nnnn,
//    rule: SMLALD,
//    safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE],
//    uses: {RdHi, RdLo, Rm, Rn}}
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary4RegisterDualResultNoCondsUpdate,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdHi, RdLo},
//    fields: [cond(31:28),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      M(5),
//      Rn(3:0)],
//    pattern: cccc01110100hhhhllllmmmm01m1nnnn,
//    rule: SMLSLD,
//    safety: [Pc in {RdHi, RdLo, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE],
//    uses: {RdHi, RdLo, Rm, Rn}}
class Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SMLALD_cccc01110100hhhhllllmmmm00m1nnnn_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary4RegisterDualResultLtV6RdHiLoNotRn,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      Rn(3:0)],
//    pattern: cccc0000111shhhhllllmmmm1001nnnn,
//    rule: SMLAL_A1,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE,
//      (ArchVersion()  <
//            6 &&
//         (RdHi  ==
//            Rn ||
//         RdLo  ==
//            Rn)) => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {RdLo, RdHi, Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary4RegisterDualResultLtV6RdHiLoNotRn,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      Rn(3:0)],
//    pattern: cccc0000101shhhhllllmmmm1001nnnn,
//    rule: UMLAL_A1,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE,
//      (ArchVersion()  <
//            6 &&
//         (RdHi  ==
//            Rn ||
//         RdLo  ==
//            Rn)) => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {RdLo, RdHi, Rn, Rm}}
class Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1);
};

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
//
// Baseline:
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltA,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(6), N(5), Rn(3:0)],
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010110dddd0000mmmm1xx0nnnn,
//    rule: SMULBB_SMULBT_SMULTB_SMULTT,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltA,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(6), N(5), Rn(3:0)],
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010010dddd0000mmmm1x10nnnn,
//    rule: SMULWB_SMULWT,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Binary3RegisterOpAltA,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), Rn(3:0)],
//    pattern: cccc01111000dddd1111mmmm0001nnnn,
//    rule: USAD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1);
};

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
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary4RegisterDualResultUsesRnRm,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      Rn(3:0)],
//    pattern: cccc0000110shhhhllllmmmm1001nnnn,
//    rule: SMULL_A1,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE,
//      (ArchVersion()  <
//            6 &&
//         (RdHi  ==
//            Rn ||
//         RdLo  ==
//            Rn)) => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm}}
//
// Baseline:
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    baseline: Binary4RegisterDualResultUsesRnRm,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28),
//      S(20),
//      RdHi(19:16),
//      RdLo(15:12),
//      Rm(11:8),
//      Rn(3:0)],
//    pattern: cccc0000100shhhhllllmmmm1001nnnn,
//    rule: UMULL_A1,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE,
//      (ArchVersion()  <
//            6 &&
//         (RdHi  ==
//            Rn ||
//         RdLo  ==
//            Rn)) => UNPREDICTABLE],
//    setflags: S(20)=1,
//    uses: {Rn, Rm}}
class Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1
     : public ClassDecoder {
 public:
  Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: StoreRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rn
//         if wback
//         else None},
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100000w0nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: STMDA_STMED,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) &&
//         Rn  !=
//            SmallestGPR(registers) => UNKNOWN],
//    small_imm_base_wb: true,
//    true: true,
//    uses: Union({Rn}, registers),
//    wback: W(21)=1}
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: StoreRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rn
//         if wback
//         else None},
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100100w0nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: STMDB_STMFD,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) &&
//         Rn  !=
//            SmallestGPR(registers) => UNKNOWN],
//    small_imm_base_wb: true,
//    true: true,
//    uses: Union({Rn}, registers),
//    wback: W(21)=1}
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: StoreRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rn
//         if wback
//         else None},
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100110w0nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: STMIB_STMFA,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) &&
//         Rn  !=
//            SmallestGPR(registers) => UNKNOWN],
//    small_imm_base_wb: true,
//    true: true,
//    uses: Union({Rn}, registers),
//    wback: W(21)=1}
//
// Baseline:
//   {None: 32,
//    Pc: 15,
//    Rn: Rn(19:16),
//    W: W(21),
//    base: Rn,
//    baseline: StoreRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rn
//         if wback
//         else None},
//    fields: [cond(31:28), W(21), Rn(19:16), register_list(15:0)],
//    pattern: cccc100010w0nnnnrrrrrrrrrrrrrrrr,
//    register_list: register_list(15:0),
//    registers: RegisterList(register_list),
//    rule: STM_STMIA_STMEA,
//    safety: [Rn  ==
//            Pc ||
//         NumGPRs(registers)  <
//            1 => UNPREDICTABLE,
//      wback &&
//         Contains(registers, Rn) &&
//         Rn  !=
//            SmallestGPR(registers) => UNKNOWN],
//    small_imm_base_wb: true,
//    true: true,
//    uses: Union({Rn}, registers),
//    wback: W(21)=1}
class Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1
     : public ClassDecoder {
 public:
  Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Store2RegisterImm12Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ZeroExtend(imm12, 32),
//    index: P(24)=1,
//    pattern: cccc010pu1w0nnnnttttiiiiiiiiiiii,
//    rule: STRB_immediate,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rt  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rn, Rt},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Store3RegisterImm5Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    index: P(24)=1,
//    pattern: cccc011pu1w0nnnnttttiiiiitt0mmmm,
//    rule: STRB_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rm  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rn  ==
//            Rm => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm, Rn, Rt},
//    wback: P(24)=0 ||
//         W(21)=1}
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Store3RegisterImm5Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm5(11:7),
//      type(6:5),
//      Rm(3:0)],
//    imm5: imm5(11:7),
//    index: P(24)=1,
//    pattern: cccc011pd0w0nnnnttttiiiiitt0mmmm,
//    rule: STR_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rm  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rn  ==
//            Rm => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rm, Rn, Rt},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    address: offset_addr
//         if index
//         else Rn,
//    base: Rn,
//    baseline: Store2RegisterImm8DoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    index: P(24)=1,
//    offset_addr: Rn + imm32
//         if add
//         else Rn - imm32,
//    pattern: cccc000pu1w0nnnnttttiiii1111iiii,
//    rule: STRD_immediate,
//    safety: [Rt(0)=1 => UNPREDICTABLE,
//      P(24)=0 &&
//         W(21)=1 => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      Rt2  ==
//            Pc => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rt, Rt2, Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1
     : public ClassDecoder {
 public:
  Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Store3RegisterDoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      Rm(3:0)],
//    index: P(24)=1,
//    pattern: cccc000pu0w0nnnntttt00001111mmmm,
//    rule: STRD_register,
//    safety: [Rt(0)=1 => UNPREDICTABLE,
//      P(24)=0 &&
//         W(21)=1 => UNPREDICTABLE,
//      Rt2  ==
//            Pc ||
//         Rm  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt ||
//         Rn  ==
//            Rt2) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rm  ==
//            Rn => UNPREDICTABLE,
//      index => FORBIDDEN],
//    uses: {Rt, Rt2, Rn, Rm},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    Rt: Rt(3:0),
//    arch: v6K,
//    base: Rn,
//    baseline: StoreExclusive3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rt(3:0)],
//    imm32: Zeros((32)),
//    pattern: cccc00011100nnnndddd11111001tttt,
//    rule: STREXB,
//    safety: [Pc in {Rd, Rt, Rn} => UNPREDICTABLE,
//      Rd in {Rn, Rt} => UNPREDICTABLE],
//    uses: {Rn, Rt}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    Rt: Rt(3:0),
//    arch: v6K,
//    base: Rn,
//    baseline: StoreExclusive3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rt(3:0)],
//    imm32: Zeros((32)),
//    pattern: cccc00011110nnnndddd11111001tttt,
//    rule: STREXH,
//    safety: [Pc in {Rd, Rt, Rn} => UNPREDICTABLE,
//      Rd in {Rn, Rt} => UNPREDICTABLE],
//    uses: {Rn, Rt}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    Rt: Rt(3:0),
//    arch: v6,
//    base: Rn,
//    baseline: StoreExclusive3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rt(3:0)],
//    imm32: Zeros((32)),
//    pattern: cccc00011000nnnndddd11111001tttt,
//    rule: STREX,
//    safety: [Pc in {Rd, Rt, Rn} => UNPREDICTABLE,
//      Rd in {Rn, Rt} => UNPREDICTABLE],
//    uses: {Rn, Rt}}
class Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1
     : public ClassDecoder {
 public:
  Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STREXB_cccc00011100nnnndddd11111001tttt_case_1);
};

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
//
// Baseline:
//   {Lr: 14,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    Rt: Rt(3:0),
//    Rt2: Rt + 1,
//    arch: v6K,
//    base: Rn,
//    baseline: StoreExclusive3RegisterDoubleOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), Rt(3:0)],
//    imm32: Zeros((32)),
//    pattern: cccc00011010nnnndddd11111001tttt,
//    rule: STREXD,
//    safety: [Pc in {Rd, Rn} ||
//         Rt(0)=1 ||
//         Rt  ==
//            Lr => UNPREDICTABLE,
//      Rd in {Rn, Rt, Rt2} => UNPREDICTABLE],
//    uses: {Rn, Rt, Rt2}}
class Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1
     : public ClassDecoder {
 public:
  Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STREXD_cccc00011010nnnndddd11111001tttt_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    address: offset_addr
//         if index
//         else Rn,
//    base: Rn,
//    baseline: Store2RegisterImm8Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm4H(11:8),
//      imm4L(3:0)],
//    imm32: ZeroExtend(imm4H:imm4L, 32),
//    imm4H: imm4H(11:8),
//    imm4L: imm4L(3:0),
//    index: P(24)=1,
//    offset_addr: Rn + imm32
//         if add
//         else Rn - imm32,
//    pattern: cccc000pu1w0nnnnttttiiii1011iiii,
//    rule: STRH_immediate,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Rt  ==
//            Pc => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rt, Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1
     : public ClassDecoder {
 public:
  Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Store3RegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      Rm(3:0)],
//    index: P(24)=1,
//    pattern: cccc000pu0w0nnnntttt00001011mmmm,
//    rule: STRH_register,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      Pc in {Rt, Rm} => UNPREDICTABLE,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE,
//      ArchVersion()  <
//            6 &&
//         wback &&
//         Rm  ==
//            Rn => UNPREDICTABLE,
//      index => FORBIDDEN],
//    shift_n: 0,
//    shift_t: SRType_LSL(),
//    uses: {Rt, Rn, Rm},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1);
};

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
//
// Baseline:
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    add: U(23)=1,
//    base: Rn,
//    baseline: Store2RegisterImm12Op,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {base
//         if wback
//         else None},
//    fields: [cond(31:28),
//      P(24),
//      U(23),
//      W(21),
//      Rn(19:16),
//      Rt(15:12),
//      imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ZeroExtend(imm12, 32),
//    index: P(24)=1,
//    pattern: cccc010pu0w0nnnnttttiiiiiiiiiiii,
//    rule: STR_immediate,
//    safety: [P(24)=0 &&
//         W(21)=1 => DECODER_ERROR,
//      wback &&
//         (Rn  ==
//            Pc ||
//         Rn  ==
//            Rt) => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rn, Rt},
//    wback: P(24)=0 ||
//         W(21)=1}
class Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_1);
};

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
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc01101000nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: SXTAB16,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc01101010nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: SXTAB,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc01101011nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: SXTAH,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc01101100nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: UXTAB16,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc01101110nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: UXTAB,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
//
// Baseline:
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    arch: v6,
//    baseline: Binary3RegisterOpAltBNoCondUpdates,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rn(19:16), Rd(15:12), rotate(11:10), Rm(3:0)],
//    pattern: cccc01101111nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:0(2:0),
//    rule: UXTAH,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_1);
};

// Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1
//
// Actual:
//   {defs: {16},
//    sets_Z_if_clear_bits: RegIndex(test_register())  ==
//            inst(19:16) &&
//         (ARMExpandImm_C(inst(11:0)) &&
//         clears_mask())  ==
//            clears_mask(),
//    uses: {inst(19:16)}}
//
// Baseline:
//   {NZCV: 16,
//    Rn: Rn(19:16),
//    baseline: MaskedBinaryRegisterImmediateTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), imm12(11:0)],
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm_C(imm12),
//    pattern: cccc00110001nnnn0000iiiiiiiiiiii,
//    rule: TST_immediate,
//    sets_Z_if_clear_bits: Rn  ==
//            RegIndex(test_register()) &&
//         (imm32 &&
//         clears_mask())  ==
//            clears_mask(),
//    uses: {Rn}}
class Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register test_register,
                                    uint32_t clears_mask) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1);
};

// Actual_Unnamed_case_1
//
// Actual:
//   {defs: {},
//    safety: [true => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {baseline: Undefined,
//    constraints: ,
//    defs: {},
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Undefined,
//    constraints: ,
//    defs: {},
//    pattern: cccc00000101xxxxxxxxxxxx1001xxxx,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Undefined,
//    constraints: ,
//    defs: {},
//    pattern: cccc00000111xxxxxxxxxxxx1001xxxx,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
//
// Baseline:
//   {baseline: Undefined,
//    constraints: ,
//    defs: {},
//    pattern: cccc1100000xnnnnxxxxccccxxxoxxxx,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
class Actual_Unnamed_case_1
     : public ClassDecoder {
 public:
  Actual_Unnamed_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_Unnamed_case_1);
};

// Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd0101n0m0mmmm,
//    rule: VABAL_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd0111n0m0mmmm,
//    rule: VABDL_integer_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd10p0n0m0mmmm,
//    rule: VMLAL_VMLSL_integer_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd11p0n0m0mmmm,
//    rule: VMULL_integer_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0111nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VABA,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0111nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VABD,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110dssnnnndddd1000nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCEQ_register_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0011nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCGE_register_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0011nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCGT_register_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0000nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VHADD,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd0010nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VHSUB,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=11 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110d1snnnndddd1101nqm0mmmm,
//    rule: VABD_floating_point,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110dssnnnndddd1110nqm1mmmm,
//    rule: VACGE,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110dssnnnndddd1110nqm1mmmm,
//    rule: VACGT,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d0snnnndddd1101nqm0mmmm,
//    rule: VADD_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d0snnnndddd1110nqm0mmmm,
//    rule: VCEQ_register_A2,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110d0snnnndddd1110nqm0mmmm,
//    rule: VCGE_register_A2,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110d1snnnndddd1110nqm0mmmm,
//    rule: VCGT_register_A2,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    arch: ASIMDv2,
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d00nnnndddd1100nqm1mmmm,
//    rule: VFMA_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    arch: ASIMDv2,
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d10nnnndddd1100nqm1mmmm,
//    rule: VFMS_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dssnnnndddd1111nqm0mmmm,
//    rule: VMAX_floating_point,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dssnnnndddd1111nqm0mmmm,
//    rule: VMIN_floating_point,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//    rule: VMLA_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100dpsnnnndddd1101nqm1mmmm,
//    rule: VMLS_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100110d0snnnndddd1101nqm1mmmm,
//    rule: VMUL_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d0snnnndddd1111nqm1mmmm,
//    rule: VRECPS,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d1snnnndddd1111nqm1mmmm,
//    rule: VRSQRTS,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    op1_neg: size(1),
//    pattern: 111100100d1snnnndddd1101nqm0mmmm,
//    rule: VSUB_floating_point_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED,
//      size(0)=1 => UNDEFINED],
//    size: size(21:20),
//    sz: size(0),
//    uses: {}}
class Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_1);
};

// Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f110qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VABS_A1,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f010qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCEQ_immediate_0,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f001qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCGE_immediate_0,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f000qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCGT_immediate_0,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f011qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCLE_immediate_0,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd01000qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCLS,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f100qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCLT_immediate_0,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd01001qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCLZ,
//    safety: [size(19:18)=11 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1);
};

// Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=~10 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f110qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VABS_A1,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f010qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCEQ_immediate_0,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f001qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCGE_immediate_0,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f000qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCGT_immediate_0,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f011qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCLE_immediate_0,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss01dddd0f100qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCLT_immediate_0,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_CVT_F2I,
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss11dddd011ppqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCVT,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED,
//      size(19:18)=~10 => UNDEFINED],
//    size: size(19:18),
//    to_integer: op(1)=1,
//    unsigned: op(0)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2
     : public ClassDecoder {
 public:
  Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_2);
};

// Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:16)(0)=1 ||
//         inst(3:0)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 111100101dssnnnndddd0100n0m0mmmm,
//    rule: VADDHN,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      Vn(0)=1 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_1);
};

// Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 ||
//         (inst(8)=1 &&
//         inst(19:16)(0)=1) => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    is_vaddw: U(24)=1,
//    is_w: op(8)=1,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd000pn0m0mmmm,
//    rule: VADDL_VADDW,
//    safety: [size(21:20)=11 => DECODER_ERROR,
//      Vd(0)=1 ||
//         (op(8)=1 &&
//         Vn(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_1);
};

// Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(19:16)(0)=1 ||
//         inst(15:12)(0)=1) => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100dssnnnndddd1000nqm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VADD_integer,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100d00nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VAND_register,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100d01nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VBIC_register,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110d11nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VBIF,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110d10nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VBIT,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110d01nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VBSL,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100110d00nnnndddd0001nqm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VEOR,
//    safety: [Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vn(0)=1 ||
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_1);
};

// Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1
//
// Actual:
//   {safety: [inst(11:8)(0)=0 ||
//         inst(11:8)(3:2)=11 => DECODER_ERROR,
//      inst(6)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED]}
//
// Baseline:
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_BIT,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
//    i: i(24),
//    imm3: imm3(18:16),
//    imm4: imm4(3:0),
//    imm64: AdvSIMDExpandImm(op, cmode, i:imm3:imm4),
//    op: op(5),
//    pattern: 1111001i1d000mmmddddcccc0q11mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VBIC_immediate,
//    safety: [cmode(0)=0 ||
//         cmode(3:2)=11 => DECODER_ERROR,
//      Q(6)=1 &&
//         Vd(0)=1 => UNDEFINED]}
//
// Baseline:
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_BIT,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
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
//         Vd(0)=1 => UNDEFINED]}
class Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1()
     : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1);
};

// Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=~00 => UNDEFINED,
//      inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//    m: M:Vm,
//    op: op(8:7),
//    pattern: 111100111d11ss00dddd01010qm0mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCNT,
//    safety: [size(19:18)=~00 => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    size: size(19:18),
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VCNT_111100111d11ss00dddd01010qm0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    arch: ASIMD,
//    baseline: VectorBinary2RegisterShiftAmount_CVT,
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2,
//    esize: 32,
//    fields: [U(24),
//      D(22),
//      imm6(21:16),
//      Vd(15:12),
//      op(8),
//      L(7),
//      Q(6),
//      M(5),
//      Vm(3:0)],
//    frac_bits: 64 - imm6,
//    imm6: imm6(21:16),
//    n: M:Vm,
//    op: op(8),
//    pattern: 1111001u1diiiiiidddd111p0qm1mmmm,
//    regs: 1
//         if Q(6)=0
//         else 2,
//    rule: VCVT_between_floating_point_and_fixed_point,
//    safety: [imm6(21:16)=000xxx => DECODER_ERROR,
//      imm6(21:16)=0xxxxx => UNDEFINED,
//      Q(6)=1 &&
//         (Vd(0)=1 ||
//         Vm(0)=1) => UNDEFINED],
//    to_fixed: op(8)=1,
//    unsigned: U(24)=1,
//    uses: {}}
class Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_1);
};

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
//
// Baseline:
//   {B: B(22),
//    D: D(7),
//    E: E(5),
//    Pc: 15,
//    Q: Q(21),
//    Rt: Rt(15:12),
//    Vd: Vd(19:16),
//    arch: AdvSIMD,
//    baseline: DuplicateToAdvSIMDRegisters,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    elements: 2
//         if B:E(1:0)=00
//         else 4
//         if B:E(1:0)=01
//         else 8
//         if B:E(1:0)=10
//         else 0,
//    esize: 32
//         if B:E(1:0)=00
//         else 16
//         if B:E(1:0)=01
//         else 8
//         if B:E(1:0)=10
//         else 0,
//    fields: [cond(31:28),
//      B(22),
//      Q(21),
//      Vd(19:16),
//      Rt(15:12),
//      D(7),
//      E(5)],
//    pattern: cccc11101bq0ddddtttt1011d0e10000,
//    regs: 1
//         if Q(21)=0
//         else 2,
//    rule: VDUP_arm_core_register,
//    safety: [Q(21)=1 &&
//         Vd(0)=1 => UNDEFINED,
//      B:E(1:0)=11 => UNDEFINED,
//      t  ==
//            Pc => UNPREDICTABLE],
//    sel: B:E,
//    t: Rt,
//    uses: {Rt}}
class Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1
     : public ClassDecoder {
 public:
  Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD1_multiple_single_elements_111101000d10nnnnddddttttssaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD1_single_element_to_all_lanes_111101001d10nnnndddd1100sstammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD1_single_element_to_one_lane_111101001d10nnnnddddss00aaaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD2_multiple_2_element_structures_111101000d10nnnnddddttttssaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD2_single_2_element_structure_to_all_lanes_111101001d10nnnndddd1101sstammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD2_single_2_element_structure_to_one_lane_111101001d10nnnnddddss01aaaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD3_multiple_3_element_structures_111101000d10nnnnddddttttssaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD3_single_3_element_structure_to_all_lanes_111101001d10nnnndddd1110sstammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD3_single_3_element_structure_to_one_lane_111101001d10nnnnddddss10aaaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    type: type(11:8),
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD4_multiple_4_element_structures_111101000d10nnnnddddttttssaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    T: T(5),
//    Vd: Vd(15:12),
//    a: a(4),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD4_single_4_element_structure_to_all_lanes_111101001d10nnnndddd1111sstammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Sp: 13,
//    Vd: Vd(15:12),
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
//    small_imm_base_wb: not register_index,
//    uses: {m
//         if wback
//         else None, n},
//    wback: (m  !=
//            Pc)}
class Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1
     : public ClassDecoder {
 public:
  Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLD4_single_4_element_structure_to_one_lane_111101001d10nnnnddddss11aaaammmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
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
//    imm32: ZeroExtend(imm8:0(1:0), 32),
//    imm8: imm8(7:0),
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
class Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLDM_cccc110pudw1nnnndddd1010iiiiiiii_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
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
//    imm32: ZeroExtend(imm8:0(1:0), 32),
//    imm8: imm8(7:0),
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
//            16 => UNPREDICTABLE],
//    single_regs: false,
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: W(21)=1}
class Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLDM_cccc110pudw1nnnndddd1011iiiiiiii_case_1);
};

// Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1
//
// Actual:
//   {base: inst(19:16),
//    defs: {},
//    is_literal_load: 15  ==
//            inst(19:16),
//    uses: {inst(19:16)}}
//
// Baseline:
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    add: U(23)=1,
//    arch: VFPv2,
//    base: Rn,
//    baseline: LoadVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
//    imm8: imm8(7:0),
//    is_literal_load: Rn  ==
//            Pc,
//    n: Rn,
//    pattern: cccc1101ud01nnnndddd1010iiiiiiii,
//    rule: VLDR,
//    single_reg: true,
//    true: true,
//    uses: {Rn}}
//
// Baseline:
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
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
//    imm32: ZeroExtend(imm8:0(1:0), 32),
//    imm8: imm8(7:0),
//    is_literal_load: Rn  ==
//            Pc,
//    n: Rn,
//    pattern: cccc1101ud01nnnndddd1011iiiiiiii,
//    rule: VLDR,
//    single_reg: false,
//    uses: {Rn}}
class Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual bool is_literal_load(Instruction i) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VLDR_cccc1101ud01nnnndddd1010iiiiiiii_case_1);
};

// Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [(inst(21:20)=00 ||
//         inst(15:12)(0)=1) => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMLAL_by_scalar_A2_1111001u1dssnnnndddd0p10n1m0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    F: F(8),
//    M: M(5),
//    N: N(7),
//    Q: Q(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2
     : public ClassDecoder {
 public:
  Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMLA_by_scalar_A1_1111001q1dssnnnndddd0p0fn1m0mmmm_case_2);
};

// Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 => UNDEFINED,
//      inst(3:0)(0)=1 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMOVN_111100111d11ss10dddd001000m0mmmm_case_1);
};

// Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE,
//      inst(22:21):inst(6:5)(3:0)=0x10 => UNDEFINED],
//    uses: {inst(15:12)}}
//
// Baseline:
//   {D: D(7),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Vd: Vd(19:16),
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
class Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1
     : public ClassDecoder {
 public:
  Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMOV_ARM_core_register_to_scalar_cccc11100ii0ddddtttt1011dii10000_case_1);
};

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
//
// Baseline:
//   {N: N(7),
//    None: 32,
//    Pc: 15,
//    Rt: Rt(15:12),
//    Vn: Vn(19:16),
//    arch: VFPv2,
//    baseline: MoveVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt
//         if to_arm_register
//         else None},
//    fields: [cond(31:28), op(20), Vn(19:16), Rt(15:12), N(7)],
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
//
// Baseline:
//   {N: N(7),
//    None: 32,
//    Pc: 15,
//    Rt: Rt(15:12),
//    Vn: Vn(19:16),
//    arch: VFPv2,
//    baseline: MoveVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt
//         if to_arm_register
//         else None},
//    fields: [cond(31:28), op(20), Vn(19:16), Rt(15:12), N(7)],
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
class Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1
     : public ClassDecoder {
 public:
  Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMOV_between_ARM_core_register_and_single_precision_register_cccc1110000onnnntttt1010n0010000_case_1);
};

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
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Rt2: Rt2(19:16),
//    Vm: Vm(3:0),
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: MoveDoubleVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2}
//         if to_arm_registers
//         else {},
//    fields: [cond(31:28), op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
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
class Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMOV_between_two_ARM_core_registers_and_a_doubleword_extension_register_cccc1100010otttttttt101100m1mmmm_case_1);
};

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
//
// Baseline:
//   {M: M(5),
//    Pc: 15,
//    Rt: Rt(15:12),
//    Rt2: Rt2(19:16),
//    Vm: Vm(3:0),
//    arch: ['VFPv2'],
//    baseline: MoveDoubleVfpRegisterOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rt, Rt2}
//         if to_arm_registers
//         else {},
//    fields: [cond(31:28), op(20), Rt2(19:16), Rt(15:12), M(5), Vm(3:0)],
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
class Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMOV_between_two_ARM_core_registers_and_two_single_precision_registers_cccc1100010otttttttt101000m1mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_MOV,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    false: false,
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
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
//    single_register: false}
class Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1()
     : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMOV_immediate_A1_1111001m1d000mmmddddcccc0qp1mmmm_case_1);
};

// Actual_VMRS_cccc111011110001tttt101000010000_case_1
//
// Actual:
//   {defs: {16
//         if 15  ==
//            inst(15:12)
//         else inst(15:12)}}
//
// Baseline:
//   {NZCV: 16,
//    Pc: 15,
//    Rt: Rt(15:12),
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: VfpMrsOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV
//         if t  ==
//            Pc
//         else Rt},
//    fields: [cond(31:28), Rt(15:12)],
//    pattern: cccc111011110001tttt101000010000,
//    rule: VMRS,
//    t: Rt}
class Actual_VMRS_cccc111011110001tttt101000010000_case_1
     : public ClassDecoder {
 public:
  Actual_VMRS_cccc111011110001tttt101000010000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMRS_cccc111011110001tttt101000010000_case_1);
};

// Actual_VMSR_cccc111011100001tttt101000010000_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(15:12) => UNPREDICTABLE],
//    uses: {inst(15:12)}}
//
// Baseline:
//   {Pc: 15,
//    Rt: Rt(15:12),
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: VfpUsesRegOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28), Rt(15:12)],
//    pattern: cccc111011100001tttt101000010000,
//    rule: VMSR,
//    safety: [t  ==
//            Pc => UNPREDICTABLE],
//    t: Rt,
//    uses: {Rt}}
class Actual_VMSR_cccc111011100001tttt101000010000_case_1
     : public ClassDecoder {
 public:
  Actual_VMSR_cccc111011100001tttt101000010000_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMSR_cccc111011100001tttt101000010000_case_1);
};

// Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR,
//      inst(24)=1 ||
//         inst(21:20)=~00 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMULL_polynomial_A2_1111001u1dssnnnndddd11p0n0m0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      size(21:20)=~00 => UNDEFINED],
//    size: size(21:20),
//    unsigned: false,
//    uses: {}}
class Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMUL_polynomial_A1_1111001u0dssnnnndddd1001nqm1mmmm_case_1);
};

// Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1
//
// Actual:
//   {safety: [(inst(11:8)(0)=1 &&
//         inst(11:8)(3:2)=~11) ||
//         inst(11:8)(3:1)=111 => DECODER_ERROR,
//      inst(6)=1 &&
//         inst(15:12)(0)=1 => UNDEFINED]}
//
// Baseline:
//   {D: D(22),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    arch: ASIMD,
//    baseline: Vector1RegisterImmediate_MVN,
//    cmode: cmode(11:8),
//    constraints: ,
//    d: D:Vd,
//    fields: [i(24),
//      D(22),
//      imm3(18:16),
//      Vd(15:12),
//      cmode(11:8),
//      Q(6),
//      op(5),
//      imm4(3:0)],
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
//         Vd(0)=1 => UNDEFINED]}
class Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1()
     : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VMVN_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_1);
};

// Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)(0)=1 ||
//         inst(6)=1 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VPADD_floating_point_111100110d0snnnndddd1101nqm0mmmm_case_1);
};

// Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=11 => UNDEFINED,
//      inst(6)=1 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 111100100dssnnnndddd1011n0m1mmmm,
//    rule: VPADD_integer,
//    safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1010n0m0mmmm,
//    rule: VPMAX,
//    safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//    m: M:Vm,
//    n: N:Vn,
//    op: op(9),
//    pattern: 1111001u0dssnnnndddd1010n0m1mmmm,
//    rule: VPMIN,
//    safety: [size(21:20)=11 => UNDEFINED, Q(6)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VPADD_integer_111100100dssnnnndddd1011n0m1mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    arch: VFPv2,
//    base: Sp,
//    baseline: LoadVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {Sp},
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
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
//
// Baseline:
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    arch: VFPv2,
//    base: Sp,
//    baseline: StoreVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {Sp},
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
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
class Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VPOP_cccc11001d111101dddd1010iiiiiiii_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Sp,
//    baseline: LoadVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {Sp},
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
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
//            16 => UNPREDICTABLE],
//    single_regs: false,
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Sp}}
//
// Baseline:
//   {D: D(22),
//    Sp: 13,
//    Vd: Vd(15:12),
//    arch: ['VFPv2', 'AdvSIMD'],
//    base: Sp,
//    baseline: StoreVectorRegisterList,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {Sp},
//    false: false,
//    fields: [cond(31:28), D(22), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
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
//            16 => UNPREDICTABLE],
//    single_regs: false,
//    small_imm_base_wb: true,
//    true: true,
//    uses: {Sp}}
class Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VPOP_cccc11001d111101dddd1011iiiiiiii_case_1);
};

// Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:20)=00 ||
//         inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:20)=11 => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
class Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VQDMLAL_VQDMLSL_A1_111100101dssnnnndddd10p1n0m0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      (size(21:20)=11 ||
//         size(21:20)=00) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
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
//         Vd(0)=1) => UNDEFINED,
//      (size(21:20)=11 ||
//         size(21:20)=00) => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VQDMULH_A1_111100100dssnnnndddd1011nqm0mmmm_case_1);
};

// Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1
//
// Actual:
//   {safety: [inst(19:18)=11 ||
//         inst(3:0)(0)=1 => UNDEFINED,
//      inst(7:6)=00 => DECODER_ERROR]}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//    constraints: ,
//    d: D:Vd,
//    dest_unsigned: op(0)=1,
//    fields: [D(22), size(19:18), Vd(15:12), op(7:6), M(5), Vm(3:0)],
//    m: M:Vm,
//    op: op(7:6),
//    pattern: 111100111d11ss10dddd0010ppm0mmmm,
//    rule: VQMOVN,
//    safety: [op(7:6)=00 => DECODER_ERROR,
//      size(19:18)=11 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(19:18),
//    src_unsigned: op(7:6)=11}
//
// Baseline:
//   {D: D(22),
//    M: M(5),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    arch: ASIMD,
//    baseline: Vector2RegisterMiscellaneous_I16_32_64N,
//    constraints: ,
//    d: D:Vd,
//    dest_unsigned: op(0)=1,
//    fields: [D(22), size(19:18), Vd(15:12), op(7:6), M(5), Vm(3:0)],
//    m: M:Vm,
//    op: op(7:6),
//    pattern: 111100111d11ss10dddd0010ppm0mmmm,
//    rule: VQMOVUN,
//    safety: [op(7:6)=00 => DECODER_ERROR,
//      size(19:18)=11 ||
//         Vm(0)=1 => UNDEFINED],
//    size: size(19:18),
//    src_unsigned: op(7:6)=11}
class Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1()
     : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VQMOVN_111100111d11ss10dddd0010ppm0mmmm_case_1);
};

// Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:16)=000xxx => DECODER_ERROR,
//      inst(24)=0 &&
//         inst(8)=0 => DECODER_ERROR,
//      inst(3:0)(0)=1 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VQRSHRN_1111001u1diiiiiidddd100p01m1mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VQSHL_VQSHLU_immediate_1111001u1diiiiiidddd011plqm1mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VREV16_111100111d11ss00dddd000ppqm0mmmm_case_1);
};

// Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(21:16)=000xxx => DECODER_ERROR,
//      inst(3:0)(0)=1 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VRSHRN_111100101diiiiiidddd100001m1mmmm_case_1);
};

// Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(6)=1 &&
//         (inst(15:12)(0)=1 ||
//         inst(3:0)(0)=1) => UNDEFINED,
//      inst(7):inst(21:16)(6:0)=0000xxx => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VRSHR_1111001u1diiiiiidddd0010lqm1mmmm_case_1);
};

// Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(15:12)(0)=1 => UNDEFINED,
//      inst(21:16)=000xxx => DECODER_ERROR],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    L: L(7),
//    M: M(5),
//    Q: Q(6),
//    U: U(24),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VSHLL_A1_or_VMOVL_1111001u1diiiiiidddd101000m1mmmm_case_1);
};

// Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1
//
// Actual:
//   {defs: {},
//    safety: [inst(19:18)=11 ||
//         inst(15:12)(0)=1 => UNDEFINED],
//    uses: {}}
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VSHLL_A2_111100111d11ss10dddd001100m0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
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
//    imm32: ZeroExtend(imm8:0(1:0), 32),
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
//      regs  ==
//            0 ||
//         d + regs  >
//            32 => UNPREDICTABLE],
//    single_regs: true,
//    small_imm_base_wb: wback,
//    true: true,
//    uses: {Rn},
//    wback: W(21)=1}
class Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VSTM_cccc110pudw0nnnndddd1010iiiiiiii_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Sp: 13,
//    U: U(23),
//    Vd: Vd(15:12),
//    W: W(21),
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
//    imm32: ZeroExtend(imm8:0(1:0), 32),
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
//      regs  ==
//            0 ||
//         regs  >
//            16 ||
//         d + regs  >
//            32 => UNPREDICTABLE,
//      VFPSmallRegisterBank() &&
//         d + regs  >
//            16 => UNPREDICTABLE],
//    single_regs: false,
//    small_imm_base_wb: wback,
//    uses: {Rn},
//    wback: W(21)=1}
class Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VSTM_cccc110pudw0nnnndddd1011iiiiiiii_case_1);
};

// Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1
//
// Actual:
//   {defs: {},
//    safety: [15  ==
//            inst(19:16) => FORBIDDEN_OPERANDS],
//    uses: {inst(19:16)}}
//
// Baseline:
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    add: U(23)=1,
//    arch: VFPv2,
//    baseline: StoreVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: Vd:D,
//    defs: {},
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
//    imm8: imm8(7:0),
//    n: Rn,
//    pattern: cccc1101ud00nnnndddd1010iiiiiiii,
//    rule: VSTR,
//    safety: [n  ==
//            Pc => FORBIDDEN_OPERANDS],
//    single_reg: true,
//    true: true,
//    uses: {Rn}}
//
// Baseline:
//   {D: D(22),
//    Pc: 15,
//    Rn: Rn(19:16),
//    U: U(23),
//    Vd: Vd(15:12),
//    add: U(23)=1,
//    arch: ['VFPv2', 'AdvSIMD'],
//    baseline: StoreVectorRegister,
//    cond: cond(31:28),
//    constraints: ,
//    d: D:Vd,
//    defs: {},
//    false: false,
//    fields: [cond(31:28), U(23), D(22), Rn(19:16), Vd(15:12), imm8(7:0)],
//    imm32: ZeroExtend(imm8:0(1:0), 32),
//    imm8: imm8(7:0),
//    n: Rn,
//    pattern: cccc1101ud00nnnndddd1011iiiiiiii,
//    rule: VSTR,
//    safety: [n  ==
//            Pc => FORBIDDEN_OPERANDS],
//    single_reg: false,
//    uses: {Rn}}
class Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1
     : public ClassDecoder {
 public:
  Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VSTR_cccc1101ud00nnnndddd1010iiiiiiii_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VSWP_111100111d11ss10dddd00000qm0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VTRN_111100111d11ss10dddd00001qm0mmmm_case_1);
};

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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
//
// Baseline:
//   {D: D(22),
//    F: F(10),
//    M: M(5),
//    Q: Q(6),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
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
class Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1
     : public ClassDecoder {
 public:
  Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Actual_VUZP_111100111d11ss10dddd00010qm0mmmm_case_1);
};

} // namespace nacl_arm_test

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_ACTUALS_H_
