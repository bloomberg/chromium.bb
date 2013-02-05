/*
 * Copyright 2013 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_BASELINES_2_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_BASELINES_2_H_

#include "native_client/src/trusted/validator_arm/arm_helpers.h"
#include "native_client/src/trusted/validator_arm/inst_classes.h"

namespace nacl_arm_dec {

// SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0:
//
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
//    generated_baseline: SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0,
//    pattern: cccc01110101ddddaaaammmm00r1nnnn,
//    rule: SMMLA,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
class SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0
     : public ClassDecoder {
 public:
  SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMMLA_cccc01110101ddddaaaammmm00r1nnnn_case_0);
};

// SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0:
//
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
//    generated_baseline: SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0,
//    pattern: cccc01110101ddddaaaammmm11r1nnnn,
//    rule: SMMLS,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
class SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0
     : public ClassDecoder {
 public:
  SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMMLS_cccc01110101ddddaaaammmm11r1nnnn_case_0);
};

// SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0:
//
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
//    generated_baseline: SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0,
//    pattern: cccc01110101dddd1111mmmm00r1nnnn,
//    rule: SMMUL,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
class SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0
     : public ClassDecoder {
 public:
  SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMMUL_cccc01110101dddd1111mmmm00r1nnnn_case_0);
};

// SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0:
//
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
//    generated_baseline: SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0,
//    pattern: cccc01110000dddd1111mmmm00m1nnnn,
//    rule: SMUAD,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
class SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0
     : public ClassDecoder {
 public:
  SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMUAD_cccc01110000dddd1111mmmm00m1nnnn_case_0);
};

// SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0:
//
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    actual: Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1,
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltA,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(6), N(5), Rn(3:0)],
//    generated_baseline: SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0,
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010110dddd0000mmmm1xx0nnnn,
//    rule: SMULBB_SMULBT_SMULTB_SMULTT,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0
     : public ClassDecoder {
 public:
  SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_0);
};

// SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    actual: Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1,
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
//    generated_baseline: SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_0,
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
class SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_0
     : public ClassDecoder {
 public:
  SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_0);
};

// SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0:
//
//   {M: M(6),
//    N: N(5),
//    Pc: 15,
//    Rd: Rd(19:16),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    actual: Actual_SMULBB_SMULBT_SMULTB_SMULTT_cccc00010110dddd0000mmmm1xx0nnnn_case_1,
//    arch: v5TE,
//    baseline: Binary3RegisterOpAltA,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(19:16), Rm(11:8), M(6), N(5), Rn(3:0)],
//    generated_baseline: SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0,
//    m_high: M(6)=1,
//    n_high: N(5)=1,
//    pattern: cccc00010010dddd0000mmmm1x10nnnn,
//    rule: SMULWB_SMULWT,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0
     : public ClassDecoder {
 public:
  SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMULWB_SMULWT_cccc00010010dddd0000mmmm1x10nnnn_case_0);
};

// SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0:
//
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
//    generated_baseline: SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0,
//    pattern: cccc01110000dddd1111mmmm01m1nnnn,
//    rule: SMUSD,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
class SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0
     : public ClassDecoder {
 public:
  SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SMUSD_cccc01110000dddd1111mmmm01m1nnnn_case_0);
};

// SRS_1111100pu1w0110100000101000iiiii_case_0:
//
//   {arch: v6,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    generated_baseline: SRS_1111100pu1w0110100000101000iiiii_case_0,
//    pattern: 1111100pu1w0110100000101000iiiii,
//    rule: SRS,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class SRS_1111100pu1w0110100000101000iiiii_case_0
     : public ClassDecoder {
 public:
  SRS_1111100pu1w0110100000101000iiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SRS_1111100pu1w0110100000101000iiiii_case_0);
};

// SSAT16_cccc01101010iiiidddd11110011nnnn_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Unary2RegisterSatImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), sat_imm(19:16), Rd(15:12), Rn(3:0)],
//    generated_baseline: SSAT16_cccc01101010iiiidddd11110011nnnn_case_0,
//    pattern: cccc01101010iiiidddd11110011nnnn,
//    rule: SSAT16,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(19:16),
//    saturate_to: sat_imm + 1,
//    uses: {Rn}}
class SSAT16_cccc01101010iiiidddd11110011nnnn_case_0
     : public ClassDecoder {
 public:
  SSAT16_cccc01101010iiiidddd11110011nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SSAT16_cccc01101010iiiidddd11110011nnnn_case_0);
};

// SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0:
//
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
//    generated_baseline: SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0,
//    imm5: imm5(11:7),
//    pattern: cccc0110101iiiiiddddiiiiis01nnnn,
//    rule: SSAT,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(20:16),
//    saturate_to: sat_imm + 1,
//    sh: sh(6),
//    shift: DecodeImmShift(sh:'0'(0), imm5),
//    uses: {Rn}}
class SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0
     : public ClassDecoder {
 public:
  SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SSAT_cccc0110101iiiiiddddiiiiis01nnnn_case_0);
};

// SSAX_cccc01100001nnnndddd11110101mmmm_case_0:
//
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
//    generated_baseline: SSAX_cccc01100001nnnndddd11110101mmmm_case_0,
//    pattern: cccc01100001nnnndddd11110101mmmm,
//    rule: SSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SSAX_cccc01100001nnnndddd11110101mmmm_case_0
     : public ClassDecoder {
 public:
  SSAX_cccc01100001nnnndddd11110101mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SSAX_cccc01100001nnnndddd11110101mmmm_case_0);
};

// SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0:
//
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
//    generated_baseline: SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0,
//    pattern: cccc01100001nnnndddd11110111mmmm,
//    rule: SSSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0
     : public ClassDecoder {
 public:
  SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SSSUB16_cccc01100001nnnndddd11110111mmmm_case_0);
};

// SSUB8_cccc01100001nnnndddd11111111mmmm_case_0:
//
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
//    generated_baseline: SSUB8_cccc01100001nnnndddd11111111mmmm_case_0,
//    pattern: cccc01100001nnnndddd11111111mmmm,
//    rule: SSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SSUB8_cccc01100001nnnndddd11111111mmmm_case_0
     : public ClassDecoder {
 public:
  SSUB8_cccc01100001nnnndddd11111111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SSUB8_cccc01100001nnnndddd11111111mmmm_case_0);
};

// STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0:
//
//   {arch: v5,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    generated_baseline: STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0,
//    pattern: 1111110pudw0nnnniiiiiiiiiiiiiiii,
//    rule: STC2,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STC2_1111110pudw0nnnniiiiiiiiiiiiiiii_case_0);
};

// STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0:
//
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    generated_baseline: STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0,
//    pattern: cccc110pudw0nnnnddddcccciiiiiiii,
//    rule: STC,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0
     : public ClassDecoder {
 public:
  STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STC_cccc110pudw0nnnnddddcccciiiiiiii_case_0);
};

// STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0:
//
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
//    generated_baseline: STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0,
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
class STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0
     : public ClassDecoder {
 public:
  STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STMDA_STMED_cccc100000w0nnnnrrrrrrrrrrrrrrrr_case_0);
};

// STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0:
//
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
//    generated_baseline: STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0,
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
class STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0
     : public ClassDecoder {
 public:
  STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STMDB_STMFD_cccc100100w0nnnnrrrrrrrrrrrrrrrr_case_0);
};

// STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0:
//
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
//    generated_baseline: STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0,
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
class STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0
     : public ClassDecoder {
 public:
  STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STMIB_STMFA_cccc100110w0nnnnrrrrrrrrrrrrrrrr_case_0);
};

// STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0:
//
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
//    generated_baseline: STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0,
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
class STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0
     : public ClassDecoder {
 public:
  STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STM_STMIA_STMEA_cccc100010w0nnnnrrrrrrrrrrrrrrrr_case_0);
};

// STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_0:
//
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
//    generated_baseline: STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_0,
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
class STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STRB_immediate_cccc010pu1w0nnnnttttiiiiiiiiiiii_case_0);
};

// STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_0:
//
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
//    generated_baseline: STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_0,
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
class STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_0
     : public ClassDecoder {
 public:
  STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STRB_register_cccc011pu1w0nnnnttttiiiiitt0mmmm_case_0);
};

// STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_0:
//
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    actual: Actual_STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_1,
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
//    generated_baseline: STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_0,
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
//            Rt ||
//         Rn  ==
//            Rt2) => UNPREDICTABLE,
//      Rt2  ==
//            Pc => UNPREDICTABLE],
//    small_imm_base_wb: wback,
//    uses: {Rt, Rt2, Rn},
//    wback: (P(24)=0) ||
//         (W(21)=1)}
class STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_0
     : public ClassDecoder {
 public:
  STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STRD_immediate_cccc000pu1w0nnnnttttiiii1111iiii_case_0);
};

// STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_0:
//
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    Rt2: Rt + 1,
//    U: U(23),
//    W: W(21),
//    actual: Actual_STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_1,
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
//    generated_baseline: STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_0,
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
class STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_0
     : public ClassDecoder {
 public:
  STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STRD_register_cccc000pu0w0nnnntttt00001111mmmm_case_0);
};

// STREXB_cccc00011100nnnndddd11111001tttt_case_0:
//
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
//    generated_baseline: STREXB_cccc00011100nnnndddd11111001tttt_case_0,
//    imm32: Zeros((32)),
//    pattern: cccc00011100nnnndddd11111001tttt,
//    rule: STREXB,
//    safety: [Pc in {Rd, Rt, Rn} => UNPREDICTABLE,
//      Rd in {Rn, Rt} => UNPREDICTABLE],
//    uses: {Rn, Rt}}
class STREXB_cccc00011100nnnndddd11111001tttt_case_0
     : public ClassDecoder {
 public:
  STREXB_cccc00011100nnnndddd11111001tttt_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STREXB_cccc00011100nnnndddd11111001tttt_case_0);
};

// STREXD_cccc00011010nnnndddd11111001tttt_case_0:
//
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
//    generated_baseline: STREXD_cccc00011010nnnndddd11111001tttt_case_0,
//    imm32: Zeros((32)),
//    pattern: cccc00011010nnnndddd11111001tttt,
//    rule: STREXD,
//    safety: [Pc in {Rd, Rn} ||
//         Rt(0)=1 ||
//         Rt  ==
//            Lr => UNPREDICTABLE,
//      Rd in {Rn, Rt, Rt2} => UNPREDICTABLE],
//    uses: {Rn, Rt, Rt2}}
class STREXD_cccc00011010nnnndddd11111001tttt_case_0
     : public ClassDecoder {
 public:
  STREXD_cccc00011010nnnndddd11111001tttt_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STREXD_cccc00011010nnnndddd11111001tttt_case_0);
};

// STREXH_cccc00011110nnnndddd11111001tttt_case_0:
//
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
//    generated_baseline: STREXH_cccc00011110nnnndddd11111001tttt_case_0,
//    imm32: Zeros((32)),
//    pattern: cccc00011110nnnndddd11111001tttt,
//    rule: STREXH,
//    safety: [Pc in {Rd, Rt, Rn} => UNPREDICTABLE,
//      Rd in {Rn, Rt} => UNPREDICTABLE],
//    uses: {Rn, Rt}}
class STREXH_cccc00011110nnnndddd11111001tttt_case_0
     : public ClassDecoder {
 public:
  STREXH_cccc00011110nnnndddd11111001tttt_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STREXH_cccc00011110nnnndddd11111001tttt_case_0);
};

// STREXH_cccc00011111nnnntttt111110011111_case_0:
//
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
//    generated_baseline: STREXH_cccc00011111nnnntttt111110011111_case_0,
//    imm32: Zeros((32)),
//    pattern: cccc00011111nnnntttt111110011111,
//    rule: STREXH,
//    safety: [Pc in {Rt, Rn} => UNPREDICTABLE],
//    uses: {Rn}}
class STREXH_cccc00011111nnnntttt111110011111_case_0
     : public ClassDecoder {
 public:
  STREXH_cccc00011111nnnntttt111110011111_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STREXH_cccc00011111nnnntttt111110011111_case_0);
};

// STREX_cccc00011000nnnndddd11111001tttt_case_0:
//
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
//    generated_baseline: STREX_cccc00011000nnnndddd11111001tttt_case_0,
//    imm32: Zeros((32)),
//    pattern: cccc00011000nnnndddd11111001tttt,
//    rule: STREX,
//    safety: [Pc in {Rd, Rt, Rn} => UNPREDICTABLE,
//      Rd in {Rn, Rt} => UNPREDICTABLE],
//    uses: {Rn, Rt}}
class STREX_cccc00011000nnnndddd11111001tttt_case_0
     : public ClassDecoder {
 public:
  STREX_cccc00011000nnnndddd11111001tttt_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STREX_cccc00011000nnnndddd11111001tttt_case_0);
};

// STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_0:
//
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    actual: Actual_STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_1,
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
//    generated_baseline: STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_0,
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
class STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_0
     : public ClassDecoder {
 public:
  STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STRH_immediate_cccc000pu1w0nnnnttttiiii1011iiii_case_0);
};

// STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_0:
//
//   {None: 32,
//    P: P(24),
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rt: Rt(15:12),
//    U: U(23),
//    W: W(21),
//    actual: Actual_STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_1,
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
//    generated_baseline: STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_0,
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
class STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_0
     : public ClassDecoder {
 public:
  STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STRH_register_cccc000pu0w0nnnntttt00001011mmmm_case_0);
};

// STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_0:
//
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
//    generated_baseline: STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_0,
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
class STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool base_address_register_writeback_small_immediate(
      Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STR_immediate_cccc010pu0w0nnnnttttiiiiiiiiiiii_case_0);
};

// STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_0:
//
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
//    generated_baseline: STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_0,
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
class STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_0
     : public ClassDecoder {
 public:
  STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_0()
     : ClassDecoder() {}
  virtual Register base_address_register(Instruction i) const;
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      STR_register_cccc011pd0w0nnnnttttiiiiitt0mmmm_case_0);
};

// SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rn: Rn(19:16),
//    S: S(20),
//    actual: Actual_ADD_immediate_cccc0010100snnnnddddiiiiiiiiiiii_case_1,
//    baseline: Binary2RegisterImmediateOpAddSub,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd, NZCV
//         if setflags
//         else None},
//    fields: [cond(31:28), S(20), Rn(19:16), Rd(15:12), imm12(11:0)],
//    generated_baseline: SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0,
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
class SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SUB_immediate_cccc0010010snnnnddddiiiiiiiiiiii_case_0);
};

// SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    actual: Actual_ADC_register_cccc0000101snnnnddddiiiiitt0mmmm_case_1,
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
//    generated_baseline: SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0,
//    imm5: imm5(11:7),
//    pattern: cccc0000010snnnnddddiiiiitt0mmmm,
//    rule: SUB_register,
//    safety: [(Rd(15:12)=1111 &&
//         S(20)=1) => DECODER_ERROR,
//      Rd(15:12)=1111 => FORBIDDEN_OPERANDS],
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
class SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0
     : public ClassDecoder {
 public:
  SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SUB_register_cccc0000010snnnnddddiiiiitt0mmmm_case_0);
};

// SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    S: S(20),
//    actual: Actual_ADC_register_shifted_register_cccc0000101snnnnddddssss0tt1mmmm_case_1,
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
//    generated_baseline: SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0,
//    pattern: cccc0000010snnnnddddssss0tt1mmmm,
//    rule: SUB_register_shifted_register,
//    safety: [Pc in {Rn, Rd, Rm, Rs} => UNPREDICTABLE],
//    setflags: S(20)=1,
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
class SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0
     : public ClassDecoder {
 public:
  SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SUB_register_shifted_register_cccc0000010snnnnddddssss0tt1mmmm_case_0);
};

// SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0:
//
//   {baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    generated_baseline: SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0,
//    pattern: cccc1111iiiiiiiiiiiiiiiiiiiiiiii,
//    rule: SVC,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SVC_cccc1111iiiiiiiiiiiiiiiiiiiiiiii_case_0);
};

// SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0:
//
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
//    generated_baseline: SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0,
//    pattern: cccc01101000nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: SXTAB16,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SXTAB16_cccc01101000nnnnddddrr000111mmmm_case_0);
};

// SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0:
//
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
//    generated_baseline: SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0,
//    pattern: cccc01101010nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: SXTAB,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SXTAB_cccc01101010nnnnddddrr000111mmmm_case_0);
};

// SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0:
//
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
//    generated_baseline: SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0,
//    pattern: cccc01101011nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: SXTAH,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SXTAH_cccc01101011nnnnddddrr000111mmmm_case_0);
};

// SXTB16_cccc011010001111ddddrr000111mmmm_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    generated_baseline: SXTB16_cccc011010001111ddddrr000111mmmm_case_0,
//    pattern: cccc011010001111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: SXTB16,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class SXTB16_cccc011010001111ddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  SXTB16_cccc011010001111ddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SXTB16_cccc011010001111ddddrr000111mmmm_case_0);
};

// SXTB_cccc011010101111ddddrr000111mmmm_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    generated_baseline: SXTB_cccc011010101111ddddrr000111mmmm_case_0,
//    pattern: cccc011010101111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: SXTB,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class SXTB_cccc011010101111ddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  SXTB_cccc011010101111ddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SXTB_cccc011010101111ddddrr000111mmmm_case_0);
};

// SXTH_cccc011010111111ddddrr000111mmmm_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    generated_baseline: SXTH_cccc011010111111ddddrr000111mmmm_case_0,
//    pattern: cccc011010111111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: SXTH,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class SXTH_cccc011010111111ddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  SXTH_cccc011010111111ddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      SXTH_cccc011010111111ddddrr000111mmmm_case_0);
};

// TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0:
//
//   {NZCV: 16,
//    Rn: Rn(19:16),
//    actual: Actual_CMN_immediate_cccc00110111nnnn0000iiiiiiiiiiii_case_1,
//    baseline: BinaryRegisterImmediateTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), imm12(11:0)],
//    generated_baseline: TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0,
//    imm12: imm12(11:0),
//    imm32: ARMExpandImm_C(imm12),
//    pattern: cccc00110011nnnn0000iiiiiiiiiiii,
//    rule: TEQ_immediate,
//    uses: {Rn}}
class TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      TEQ_immediate_cccc00110011nnnn0000iiiiiiiiiiii_case_0);
};

// TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
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
//    generated_baseline: TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0,
//    imm5: imm5(11:7),
//    pattern: cccc00010011nnnn0000iiiiitt0mmmm,
//    rule: TEQ_register,
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
class TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0
     : public ClassDecoder {
 public:
  TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      TEQ_register_cccc00010011nnnn0000iiiiitt0mmmm_case_0);
};

// TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0:
//
//   {NZCV: 16,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    actual: Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1,
//    baseline: Binary3RegisterShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), Rs(11:8), type(6:5), Rm(3:0)],
//    generated_baseline: TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0,
//    pattern: cccc00010011nnnn0000ssss0tt1mmmm,
//    rule: TEQ_register_shifted_register,
//    safety: [Pc in {Rn, Rm, Rs} => UNPREDICTABLE],
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
class TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0
     : public ClassDecoder {
 public:
  TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      TEQ_register_shifted_register_cccc00010011nnnn0000ssss0tt1mmmm_case_0);
};

// TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0:
//
//   {NZCV: 16,
//    Rn: Rn(19:16),
//    actual: Actual_TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_1,
//    baseline: MaskedBinaryRegisterImmediateTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), imm12(11:0)],
//    generated_baseline: TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0,
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
class TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0
     : public ClassDecoder {
 public:
  TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual bool sets_Z_if_bits_clear(Instruction i,
                                    Register test_register,
                                    uint32_t clears_mask) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      TST_immediate_cccc00110001nnnn0000iiiiiiiiiiii_case_0);
};

// TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    S: S(20),
//    actual: Actual_CMN_register_cccc00010111nnnn0000iiiiitt0mmmm_case_1,
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
//    generated_baseline: TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0,
//    imm5: imm5(11:7),
//    pattern: cccc00010001nnnn0000iiiiitt0mmmm,
//    rule: TST_register,
//    shift: DecodeImmShift(type, imm5),
//    type: type(6:5),
//    uses: {Rn, Rm}}
class TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0
     : public ClassDecoder {
 public:
  TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      TST_register_cccc00010001nnnn0000iiiiitt0mmmm_case_0);
};

// TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0:
//
//   {NZCV: 16,
//    Pc: 15,
//    Rm: Rm(3:0),
//    Rn: Rn(19:16),
//    Rs: Rs(11:8),
//    actual: Actual_CMN_register_shifted_register_cccc00010111nnnn0000ssss0tt1mmmm_case_1,
//    baseline: Binary3RegisterShiftedTest,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {NZCV},
//    fields: [cond(31:28), Rn(19:16), Rs(11:8), type(6:5), Rm(3:0)],
//    generated_baseline: TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0,
//    pattern: cccc00010001nnnn0000ssss0tt1mmmm,
//    rule: TST_register_shifted_register,
//    safety: [Pc in {Rn, Rm, Rs} => UNPREDICTABLE],
//    shift_t: DecodeRegShift(type),
//    type: type(6:5),
//    uses: {Rn, Rm, Rs}}
class TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0
     : public ClassDecoder {
 public:
  TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      TST_register_shifted_register_cccc00010001nnnn0000ssss0tt1mmmm_case_0);
};

// UADD16_cccc01100101nnnndddd11110001mmmm_case_0:
//
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
//    generated_baseline: UADD16_cccc01100101nnnndddd11110001mmmm_case_0,
//    pattern: cccc01100101nnnndddd11110001mmmm,
//    rule: UADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UADD16_cccc01100101nnnndddd11110001mmmm_case_0
     : public ClassDecoder {
 public:
  UADD16_cccc01100101nnnndddd11110001mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UADD16_cccc01100101nnnndddd11110001mmmm_case_0);
};

// UADD8_cccc01100101nnnndddd11111001mmmm_case_0:
//
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
//    generated_baseline: UADD8_cccc01100101nnnndddd11111001mmmm_case_0,
//    pattern: cccc01100101nnnndddd11111001mmmm,
//    rule: UADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UADD8_cccc01100101nnnndddd11111001mmmm_case_0
     : public ClassDecoder {
 public:
  UADD8_cccc01100101nnnndddd11111001mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UADD8_cccc01100101nnnndddd11111001mmmm_case_0);
};

// UASX_cccc01100101nnnndddd11110011mmmm_case_0:
//
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
//    generated_baseline: UASX_cccc01100101nnnndddd11110011mmmm_case_0,
//    pattern: cccc01100101nnnndddd11110011mmmm,
//    rule: UASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UASX_cccc01100101nnnndddd11110011mmmm_case_0
     : public ClassDecoder {
 public:
  UASX_cccc01100101nnnndddd11110011mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UASX_cccc01100101nnnndddd11110011mmmm_case_0);
};

// UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6T2,
//    baseline: Binary2RegisterBitRangeNotRnIsPcBitfieldExtract,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), widthm1(20:16), Rd(15:12), lsb(11:7), Rn(3:0)],
//    generated_baseline: UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0,
//    lsb: lsb(11:7),
//    pattern: cccc0111111mmmmmddddlllll101nnnn,
//    rule: UBFX,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE,
//      lsb + widthm1  >
//            31 => UNPREDICTABLE],
//    uses: {Rn},
//    widthm1: widthm1(20:16)}
class UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0
     : public ClassDecoder {
 public:
  UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UBFX_cccc0111111mmmmmddddlllll101nnnn_case_0);
};

// UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0:
//
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
//    generated_baseline: UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0,
//    pattern: cccc01110011dddd1111mmmm0001nnnn,
//    rule: UDIV,
//    safety: [Pc in {Rd, Rm, Rn} => UNPREDICTABLE],
//    uses: {Rm, Rn}}
class UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0
     : public ClassDecoder {
 public:
  UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UDIV_cccc01110011dddd1111mmmm0001nnnn_case_0);
};

// UHADD16_cccc01100111nnnndddd11110001mmmm_case_0:
//
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
//    generated_baseline: UHADD16_cccc01100111nnnndddd11110001mmmm_case_0,
//    pattern: cccc01100111nnnndddd11110001mmmm,
//    rule: UHADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UHADD16_cccc01100111nnnndddd11110001mmmm_case_0
     : public ClassDecoder {
 public:
  UHADD16_cccc01100111nnnndddd11110001mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UHADD16_cccc01100111nnnndddd11110001mmmm_case_0);
};

// UHADD8_cccc01100111nnnndddd11111001mmmm_case_0:
//
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
//    generated_baseline: UHADD8_cccc01100111nnnndddd11111001mmmm_case_0,
//    pattern: cccc01100111nnnndddd11111001mmmm,
//    rule: UHADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UHADD8_cccc01100111nnnndddd11111001mmmm_case_0
     : public ClassDecoder {
 public:
  UHADD8_cccc01100111nnnndddd11111001mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UHADD8_cccc01100111nnnndddd11111001mmmm_case_0);
};

// UHASX_cccc01100111nnnndddd11110011mmmm_case_0:
//
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
//    generated_baseline: UHASX_cccc01100111nnnndddd11110011mmmm_case_0,
//    pattern: cccc01100111nnnndddd11110011mmmm,
//    rule: UHASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UHASX_cccc01100111nnnndddd11110011mmmm_case_0
     : public ClassDecoder {
 public:
  UHASX_cccc01100111nnnndddd11110011mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UHASX_cccc01100111nnnndddd11110011mmmm_case_0);
};

// UHSAX_cccc01100111nnnndddd11110101mmmm_case_0:
//
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
//    generated_baseline: UHSAX_cccc01100111nnnndddd11110101mmmm_case_0,
//    pattern: cccc01100111nnnndddd11110101mmmm,
//    rule: UHSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UHSAX_cccc01100111nnnndddd11110101mmmm_case_0
     : public ClassDecoder {
 public:
  UHSAX_cccc01100111nnnndddd11110101mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UHSAX_cccc01100111nnnndddd11110101mmmm_case_0);
};

// UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0:
//
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
//    generated_baseline: UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0,
//    pattern: cccc01100111nnnndddd11110111mmmm,
//    rule: UHSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0
     : public ClassDecoder {
 public:
  UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UHSUB16_cccc01100111nnnndddd11110111mmmm_case_0);
};

// UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0:
//
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
//    generated_baseline: UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0,
//    pattern: cccc01100111nnnndddd11111111mmmm,
//    rule: UHSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0
     : public ClassDecoder {
 public:
  UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UHSUB8_cccc01100111nnnndddd11111111mmmm_case_0);
};

// UMAAL_A1_cccc00000100hhhhllllmmmm1001nnnn_case_0:
//
//   {Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    actual: Actual_SMLALBB_SMLALBT_SMLALTB_SMLALTT_cccc00010100hhhhllllmmmm1xx0nnnn_case_1,
//    baseline: Binary4RegisterDualResult,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {RdLo, RdHi},
//    fields: [cond(31:28), RdHi(19:16), RdLo(15:12), Rm(11:8), Rn(3:0)],
//    generated_baseline: UMAAL_A1_cccc00000100hhhhllllmmmm1001nnnn_case_0,
//    pattern: cccc00000100hhhhllllmmmm1001nnnn,
//    rule: UMAAL_A1,
//    safety: [Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE,
//      RdHi  ==
//            RdLo => UNPREDICTABLE],
//    uses: {RdLo, RdHi, Rn, Rm}}
class UMAAL_A1_cccc00000100hhhhllllmmmm1001nnnn_case_0
     : public ClassDecoder {
 public:
  UMAAL_A1_cccc00000100hhhhllllmmmm1001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UMAAL_A1_cccc00000100hhhhllllmmmm1001nnnn_case_0);
};

// UMLAL_A1_cccc0000101shhhhllllmmmm1001nnnn_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    actual: Actual_SMLAL_A1_cccc0000111shhhhllllmmmm1001nnnn_case_1,
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
//    generated_baseline: UMLAL_A1_cccc0000101shhhhllllmmmm1001nnnn_case_0,
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
class UMLAL_A1_cccc0000101shhhhllllmmmm1001nnnn_case_0
     : public ClassDecoder {
 public:
  UMLAL_A1_cccc0000101shhhhllllmmmm1001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UMLAL_A1_cccc0000101shhhhllllmmmm1001nnnn_case_0);
};

// UMULL_A1_cccc0000100shhhhllllmmmm1001nnnn_case_0:
//
//   {NZCV: 16,
//    None: 32,
//    Pc: 15,
//    RdHi: RdHi(19:16),
//    RdLo: RdLo(15:12),
//    Rm: Rm(11:8),
//    Rn: Rn(3:0),
//    S: S(20),
//    actual: Actual_SMULL_A1_cccc0000110shhhhllllmmmm1001nnnn_case_1,
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
//    generated_baseline: UMULL_A1_cccc0000100shhhhllllmmmm1001nnnn_case_0,
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
class UMULL_A1_cccc0000100shhhhllllmmmm1001nnnn_case_0
     : public ClassDecoder {
 public:
  UMULL_A1_cccc0000100shhhhllllmmmm1001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UMULL_A1_cccc0000100shhhhllllmmmm1001nnnn_case_0);
};

// UQADD16_cccc01100110nnnndddd11110001mmmm_case_0:
//
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
//    generated_baseline: UQADD16_cccc01100110nnnndddd11110001mmmm_case_0,
//    pattern: cccc01100110nnnndddd11110001mmmm,
//    rule: UQADD16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UQADD16_cccc01100110nnnndddd11110001mmmm_case_0
     : public ClassDecoder {
 public:
  UQADD16_cccc01100110nnnndddd11110001mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UQADD16_cccc01100110nnnndddd11110001mmmm_case_0);
};

// UQADD8_cccc01100110nnnndddd11111001mmmm_case_0:
//
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
//    generated_baseline: UQADD8_cccc01100110nnnndddd11111001mmmm_case_0,
//    pattern: cccc01100110nnnndddd11111001mmmm,
//    rule: UQADD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UQADD8_cccc01100110nnnndddd11111001mmmm_case_0
     : public ClassDecoder {
 public:
  UQADD8_cccc01100110nnnndddd11111001mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UQADD8_cccc01100110nnnndddd11111001mmmm_case_0);
};

// UQASX_cccc01100110nnnndddd11110011mmmm_case_0:
//
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
//    generated_baseline: UQASX_cccc01100110nnnndddd11110011mmmm_case_0,
//    pattern: cccc01100110nnnndddd11110011mmmm,
//    rule: UQASX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UQASX_cccc01100110nnnndddd11110011mmmm_case_0
     : public ClassDecoder {
 public:
  UQASX_cccc01100110nnnndddd11110011mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UQASX_cccc01100110nnnndddd11110011mmmm_case_0);
};

// UQSAX_cccc01100110nnnndddd11110101mmmm_case_0:
//
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
//    generated_baseline: UQSAX_cccc01100110nnnndddd11110101mmmm_case_0,
//    pattern: cccc01100110nnnndddd11110101mmmm,
//    rule: UQSAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UQSAX_cccc01100110nnnndddd11110101mmmm_case_0
     : public ClassDecoder {
 public:
  UQSAX_cccc01100110nnnndddd11110101mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UQSAX_cccc01100110nnnndddd11110101mmmm_case_0);
};

// UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0:
//
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
//    generated_baseline: UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0,
//    pattern: cccc01100110nnnndddd11110111mmmm,
//    rule: UQSUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0
     : public ClassDecoder {
 public:
  UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UQSUB16_cccc01100110nnnndddd11110111mmmm_case_0);
};

// UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0:
//
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
//    generated_baseline: UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0,
//    pattern: cccc01100110nnnndddd11111111mmmm,
//    rule: UQSUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0
     : public ClassDecoder {
 public:
  UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UQSUB8_cccc01100110nnnndddd11111111mmmm_case_0);
};

// USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0:
//
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
//    generated_baseline: USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0,
//    pattern: cccc01111000dddd1111mmmm0001nnnn,
//    rule: USAD8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0
     : public ClassDecoder {
 public:
  USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USAD8_cccc01111000dddd1111mmmm0001nnnn_case_0);
};

// USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0:
//
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
//    generated_baseline: USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0,
//    pattern: cccc01111000ddddaaaammmm0001nnnn,
//    rule: USADA8,
//    safety: [Ra  ==
//            Pc => DECODER_ERROR,
//      Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm, Ra}}
class USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0
     : public ClassDecoder {
 public:
  USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USADA8_cccc01111000ddddaaaammmm0001nnnn_case_0);
};

// USAT16_cccc01101110iiiidddd11110011nnnn_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rn: Rn(3:0),
//    arch: v6,
//    baseline: Unary2RegisterSatImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), sat_imm(19:16), Rd(15:12), Rn(3:0)],
//    generated_baseline: USAT16_cccc01101110iiiidddd11110011nnnn_case_0,
//    pattern: cccc01101110iiiidddd11110011nnnn,
//    rule: USAT16,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(19:16),
//    saturate_to: sat_imm,
//    uses: {Rn}}
class USAT16_cccc01101110iiiidddd11110011nnnn_case_0
     : public ClassDecoder {
 public:
  USAT16_cccc01101110iiiidddd11110011nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USAT16_cccc01101110iiiidddd11110011nnnn_case_0);
};

// USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0:
//
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
//    generated_baseline: USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0,
//    imm5: imm5(11:7),
//    pattern: cccc0110111iiiiiddddiiiiis01nnnn,
//    rule: USAT,
//    safety: [Pc in {Rd, Rn} => UNPREDICTABLE],
//    sat_imm: sat_imm(20:16),
//    saturate_to: sat_imm,
//    sh: sh(6),
//    shift: DecodeImmShift(sh:'0'(0), imm5),
//    uses: {Rn}}
class USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0
     : public ClassDecoder {
 public:
  USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USAT_cccc0110111iiiiiddddiiiiis01nnnn_case_0);
};

// USAX_cccc01100101nnnndddd11110101mmmm_case_0:
//
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
//    generated_baseline: USAX_cccc01100101nnnndddd11110101mmmm_case_0,
//    pattern: cccc01100101nnnndddd11110101mmmm,
//    rule: USAX,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class USAX_cccc01100101nnnndddd11110101mmmm_case_0
     : public ClassDecoder {
 public:
  USAX_cccc01100101nnnndddd11110101mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USAX_cccc01100101nnnndddd11110101mmmm_case_0);
};

// USUB16_cccc01100101nnnndddd11110111mmmm_case_0:
//
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
//    generated_baseline: USUB16_cccc01100101nnnndddd11110111mmmm_case_0,
//    pattern: cccc01100101nnnndddd11110111mmmm,
//    rule: USUB16,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class USUB16_cccc01100101nnnndddd11110111mmmm_case_0
     : public ClassDecoder {
 public:
  USUB16_cccc01100101nnnndddd11110111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USUB16_cccc01100101nnnndddd11110111mmmm_case_0);
};

// USUB8_cccc01100101nnnndddd11111111mmmm_case_0:
//
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
//    generated_baseline: USUB8_cccc01100101nnnndddd11111111mmmm_case_0,
//    pattern: cccc01100101nnnndddd11111111mmmm,
//    rule: USUB8,
//    safety: [Pc in {Rd, Rn, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class USUB8_cccc01100101nnnndddd11111111mmmm_case_0
     : public ClassDecoder {
 public:
  USUB8_cccc01100101nnnndddd11111111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      USUB8_cccc01100101nnnndddd11111111mmmm_case_0);
};

// UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0:
//
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
//    generated_baseline: UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0,
//    pattern: cccc01101100nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: UXTAB16,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UXTAB16_cccc01101100nnnnddddrr000111mmmm_case_0);
};

// UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0:
//
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
//    generated_baseline: UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0,
//    pattern: cccc01101110nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: UXTAB,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UXTAB_cccc01101110nnnnddddrr000111mmmm_case_0);
};

// UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0:
//
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
//    generated_baseline: UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0,
//    pattern: cccc01101111nnnnddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: UXTAH,
//    safety: [Rn(19:16)=1111 => DECODER_ERROR,
//      Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rn, Rm}}
class UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UXTAH_cccc01101111nnnnddddrr000111mmmm_case_0);
};

// UXTB16_cccc011011001111ddddrr000111mmmm_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    generated_baseline: UXTB16_cccc011011001111ddddrr000111mmmm_case_0,
//    pattern: cccc011011001111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: UXTB16,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class UXTB16_cccc011011001111ddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  UXTB16_cccc011011001111ddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UXTB16_cccc011011001111ddddrr000111mmmm_case_0);
};

// UXTB_cccc011011101111ddddrr000111mmmm_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    generated_baseline: UXTB_cccc011011101111ddddrr000111mmmm_case_0,
//    pattern: cccc011011101111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: UXTB,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class UXTB_cccc011011101111ddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  UXTB_cccc011011101111ddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UXTB_cccc011011101111ddddrr000111mmmm_case_0);
};

// UXTH_cccc011011111111ddddrr000111mmmm_case_0:
//
//   {Pc: 15,
//    Rd: Rd(15:12),
//    Rm: Rm(3:0),
//    arch: v6,
//    baseline: Unary2RegisterImmedShiftedOp,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {Rd},
//    fields: [cond(31:28), Rd(15:12), rotate(11:10), Rm(3:0)],
//    generated_baseline: UXTH_cccc011011111111ddddrr000111mmmm_case_0,
//    pattern: cccc011011111111ddddrr000111mmmm,
//    rotate: rotate(11:10),
//    rotation: rotate:'000'(2:0),
//    rule: UXTH,
//    safety: [Pc in {Rd, Rm} => UNPREDICTABLE],
//    uses: {Rm}}
class UXTH_cccc011011111111ddddrr000111mmmm_case_0
     : public ClassDecoder {
 public:
  UXTH_cccc011011111111ddddrr000111mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      UXTH_cccc011011111111ddddrr000111mmmm_case_0);
};

// Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0:
//
//   {arch: MPExt,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0,
//    pattern: 11110100x001xxxxxxxxxxxxxxxxxxxx,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_11110100x001xxxxxxxxxxxxxxxxxxxx_case_0);
};

// Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0,
//    pattern: 11110100xx11xxxxxxxxxxxxxxxxxxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_11110100xx11xxxxxxxxxxxxxxxxxxxx_case_0);
};

// Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0,
//    pattern: 111101010011xxxxxxxxxxxxxxxxxxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_111101010011xxxxxxxxxxxxxxxxxxxx_case_0);
};

// Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0,
//    pattern: 111101010111xxxxxxxxxxxx0000xxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_111101010111xxxxxxxxxxxx0000xxxx_case_0);
};

// Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0,
//    pattern: 111101010111xxxxxxxxxxxx001xxxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_111101010111xxxxxxxxxxxx001xxxxx_case_0);
};

// Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0,
//    pattern: 111101010111xxxxxxxxxxxx0111xxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_111101010111xxxxxxxxxxxx0111xxxx_case_0);
};

// Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0,
//    pattern: 111101010111xxxxxxxxxxxx1xxxxxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_111101010111xxxxxxxxxxxx1xxxxxxx_case_0);
};

// Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0,
//    pattern: 111101011x11xxxxxxxxxxxxxxxxxxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_111101011x11xxxxxxxxxxxxxxxxxxxx_case_0);
};

// Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0,
//    pattern: 11110101x001xxxxxxxxxxxxxxxxxxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_11110101x001xxxxxxxxxxxxxxxxxxxx_case_0);
};

// Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0:
//
//   {arch: MPExt,
//    baseline: Forbidden,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0,
//    pattern: 11110110x001xxxxxxxxxxxxxxx0xxxx,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_11110110x001xxxxxxxxxxxxxxx0xxxx_case_0);
};

// Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0:
//
//   {baseline: Unpredictable,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0,
//    pattern: 1111011xxx11xxxxxxxxxxxxxxx0xxxx,
//    safety: [true => UNPREDICTABLE],
//    true: true,
//    uses: {}}
class Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_1111011xxx11xxxxxxxxxxxxxxx0xxxx_case_0);
};

// Unnamed_case_0:
//
//   {baseline: Forbidden,
//    cond: cond(31:28),
//    constraints: ,
//    defs: {},
//    fields: [cond(31:28)],
//    generated_baseline: Unnamed_case_0,
//    safety: [true => FORBIDDEN],
//    true: true,
//    uses: {}}
class Unnamed_case_0
     : public ClassDecoder {
 public:
  Unnamed_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_case_0);
};

// Unnamed_case_1:
//
//   {baseline: Undefined,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_case_1,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
class Unnamed_case_1
     : public ClassDecoder {
 public:
  Unnamed_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_case_1);
};

// Unnamed_cccc00000101xxxxxxxxxxxx1001xxxx_case_0:
//
//   {actual: Actual_Unnamed_case_1,
//    baseline: Undefined,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_cccc00000101xxxxxxxxxxxx1001xxxx_case_0,
//    pattern: cccc00000101xxxxxxxxxxxx1001xxxx,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
class Unnamed_cccc00000101xxxxxxxxxxxx1001xxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_cccc00000101xxxxxxxxxxxx1001xxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_cccc00000101xxxxxxxxxxxx1001xxxx_case_0);
};

// Unnamed_cccc00000111xxxxxxxxxxxx1001xxxx_case_0:
//
//   {actual: Actual_Unnamed_case_1,
//    baseline: Undefined,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_cccc00000111xxxxxxxxxxxx1001xxxx_case_0,
//    pattern: cccc00000111xxxxxxxxxxxx1001xxxx,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
class Unnamed_cccc00000111xxxxxxxxxxxx1001xxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_cccc00000111xxxxxxxxxxxx1001xxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_cccc00000111xxxxxxxxxxxx1001xxxx_case_0);
};

// Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0:
//
//   {baseline: Undefined,
//    constraints: ,
//    defs: {},
//    generated_baseline: Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0,
//    pattern: cccc1100000xnnnnxxxxccccxxxoxxxx,
//    safety: [true => UNDEFINED],
//    true: true,
//    uses: {}}
class Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0
     : public ClassDecoder {
 public:
  Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      Unnamed_cccc1100000xnnnnxxxxccccxxxoxxxx_case_0);
};

// VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_0:
//
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
//    generated_baseline: VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd0101n0m0mmmm,
//    rule: VABAL_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_0
     : public ClassDecoder {
 public:
  VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABAL_A2_1111001u1dssnnnndddd0101n0m0mmmm_case_0);
};

// VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0:
//
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
//    generated_baseline: VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0,
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
class VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABA_1111001u0dssnnnndddd0111nqm1mmmm_case_0);
};

// VABDL_integer_A2_1111001u1dssnnnndddd0111n0m0mmmm_case_0:
//
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
//    generated_baseline: VABDL_integer_A2_1111001u1dssnnnndddd0111n0m0mmmm_case_0,
//    m: M:Vm,
//    n: N:Vn,
//    op: op(8),
//    pattern: 1111001u1dssnnnndddd0111n0m0mmmm,
//    rule: VABDL_integer_A2,
//    safety: [size(21:20)=11 => DECODER_ERROR, Vd(0)=1 => UNDEFINED],
//    size: size(21:20),
//    unsigned: U(24)=1,
//    uses: {}}
class VABDL_integer_A2_1111001u1dssnnnndddd0111n0m0mmmm_case_0
     : public ClassDecoder {
 public:
  VABDL_integer_A2_1111001u1dssnnnndddd0111n0m0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABDL_integer_A2_1111001u1dssnnnndddd0111n0m0mmmm_case_0);
};

// VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0:
//
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
//    generated_baseline: VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0,
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
class VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABD_1111001u0dssnnnndddd0111nqm0mmmm_case_0);
};

// VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0:
//
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
//    generated_baseline: VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0,
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
class VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABD_floating_point_111100110d1snnnndddd1101nqm0mmmm_case_0);
};

// VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0:
//
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
//    generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0,
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
class VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_0);
};

// VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1:
//
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
//    generated_baseline: VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1,
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
class VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1
     : public ClassDecoder {
 public:
  VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VABS_A1_111100111d11ss01dddd0f110qm0mmmm_case_1);
};

// VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0:
//
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
//    generated_baseline: VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0,
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
class VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VACGE_111100110dssnnnndddd1110nqm1mmmm_case_0);
};

// VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0:
//
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
//    generated_baseline: VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0,
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
class VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VACGT_111100110dssnnnndddd1110nqm1mmmm_case_0);
};

// VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_0:
//
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
//    generated_baseline: VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_0,
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
class VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_0
     : public ClassDecoder {
 public:
  VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VADDHN_111100101dssnnnndddd0100n0m0mmmm_case_0);
};

// VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_0:
//
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
//    generated_baseline: VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_0,
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
class VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_0
     : public ClassDecoder {
 public:
  VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VADDL_VADDW_1111001u1dssnnnndddd000pn0m0mmmm_case_0);
};

// VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0:
//
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
//    generated_baseline: VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0,
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
class VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VADD_floating_point_A1_111100100d0snnnndddd1101nqm0mmmm_case_0);
};

// VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    advsimd: false,
//    arch: VFPv2,
//    baseline: CondVfpOp,
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
//    generated_baseline: VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0,
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    pattern: cccc11100d11nnnndddd101sn0m0mmmm,
//    rule: VADD_floating_point,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
class VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0
     : public ClassDecoder {
 public:
  VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VADD_floating_point_cccc11100d11nnnndddd101sn0m0mmmm_case_0);
};

// VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0:
//
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
//    generated_baseline: VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0,
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
class VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VADD_integer_111100100dssnnnndddd1000nqm0mmmm_case_0);
};

// VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0:
//
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
//    generated_baseline: VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0,
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
class VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VAND_register_111100100d00nnnndddd0001nqm1mmmm_case_0);
};

// VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0:
//
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
//    generated_baseline: VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0,
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
class VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0
     : public ClassDecoder {
 public:
  VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0()
     : ClassDecoder() {}
  virtual SafetyLevel safety(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VBIC_immediate_1111001i1d000mmmddddcccc0q11mmmm_case_0);
};

// VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0:
//
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
//    generated_baseline: VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0,
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
class VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VBIC_register_111100100d01nnnndddd0001nqm1mmmm_case_0);
};

// VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0:
//
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
//    generated_baseline: VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0,
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
class VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VBIF_111100110d11nnnndddd0001nqm1mmmm_case_0);
};

// VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0:
//
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
//    generated_baseline: VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0,
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
class VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VBIT_111100110d10nnnndddd0001nqm1mmmm_case_0);
};

// VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0:
//
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
//    generated_baseline: VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0,
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
class VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VBSL_111100110d01nnnndddd0001nqm1mmmm_case_0);
};

// VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0:
//
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
//    generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0,
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
class VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_0);
};

// VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1:
//
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
//    generated_baseline: VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1,
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
class VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1
     : public ClassDecoder {
 public:
  VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCEQ_immediate_0_111100111d11ss01dddd0f010qm0mmmm_case_1);
};

// VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0:
//
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
//    generated_baseline: VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0,
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
class VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCEQ_register_A1_111100110dssnnnndddd1000nqm1mmmm_case_0);
};

// VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0:
//
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
//    generated_baseline: VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0,
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
class VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCEQ_register_A2_111100100d0snnnndddd1110nqm0mmmm_case_0);
};

// VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0:
//
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
//    generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0,
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
class VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_0);
};

// VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1:
//
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
//    generated_baseline: VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1,
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
class VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1
     : public ClassDecoder {
 public:
  VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGE_immediate_0_111100111d11ss01dddd0f001qm0mmmm_case_1);
};

// VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0:
//
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
//    generated_baseline: VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0,
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
class VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGE_register_A1_1111001u0dssnnnndddd0011nqm1mmmm_case_0);
};

// VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0:
//
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
//    generated_baseline: VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0,
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
class VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGE_register_A2_111100110d0snnnndddd1110nqm0mmmm_case_0);
};

// VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0:
//
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
//    generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0,
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
class VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_0);
};

// VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1:
//
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
//    generated_baseline: VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1,
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
class VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1
     : public ClassDecoder {
 public:
  VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGT_immediate_0_111100111d11ss01dddd0f000qm0mmmm_case_1);
};

// VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0:
//
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
//    generated_baseline: VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0,
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
class VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGT_register_A1_1111001u0dssnnnndddd0011nqm0mmmm_case_0);
};

// VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0:
//
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
//    generated_baseline: VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0,
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
class VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCGT_register_A2_111100110d1snnnndddd1110nqm0mmmm_case_0);
};

// VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0:
//
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
//    generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0,
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
class VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_0);
};

// VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1:
//
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
//    generated_baseline: VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1,
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
class VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1
     : public ClassDecoder {
 public:
  VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCLE_immediate_0_111100111d11ss01dddd0f011qm0mmmm_case_1);
};

// VCLS_111100111d11ss00dddd01000qm0mmmm_case_0:
//
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
//    generated_baseline: VCLS_111100111d11ss00dddd01000qm0mmmm_case_0,
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
class VCLS_111100111d11ss00dddd01000qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCLS_111100111d11ss00dddd01000qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCLS_111100111d11ss00dddd01000qm0mmmm_case_0);
};

// VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0:
//
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
//    generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0,
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
class VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_0);
};

// VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1:
//
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
//    generated_baseline: VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1,
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
class VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1
     : public ClassDecoder {
 public:
  VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCLT_immediate_0_111100111d11ss01dddd0f100qm0mmmm_case_1);
};

// VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0:
//
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
//    generated_baseline: VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0,
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
class VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCLZ_111100111d11ss00dddd01001qm0mmmm_case_0);
};

// VCNT_111100111d11ss00dddd01010qm0mmmm_case_0:
//
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
//    generated_baseline: VCNT_111100111d11ss00dddd01010qm0mmmm_case_0,
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
class VCNT_111100111d11ss00dddd01010qm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCNT_111100111d11ss00dddd01010qm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCNT_111100111d11ss00dddd01010qm0mmmm_case_0);
};

// VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0:
//
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
//    generated_baseline: VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0,
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
class VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCVT_111100111d11ss11dddd011ppqm0mmmm_case_0);
};

// VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0:
//
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
//    generated_baseline: VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0,
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
class VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0
     : public ClassDecoder {
 public:
  VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VCVT_between_floating_point_and_fixed_point_1111001u1diiiiiidddd111p0qm1mmmm_case_0);
};

// VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    advsimd: false,
//    arch: VFPv2,
//    baseline: CondVfpOp,
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
//    generated_baseline: VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0,
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    pattern: cccc11101d00nnnndddd101sn0m0mmmm,
//    rule: VDIV,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
class VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0
     : public ClassDecoder {
 public:
  VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VDIV_cccc11101d00nnnndddd101sn0m0mmmm_case_0);
};

// VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0:
//
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
//    generated_baseline: VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0,
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
class VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0
     : public ClassDecoder {
 public:
  VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VDUP_arm_core_register_cccc11101bq0ddddtttt1011d0e10000_case_0);
};

// VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0:
//
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
//    generated_baseline: VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0,
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
class VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VEOR_111100110d00nnnndddd0001nqm1mmmm_case_0);
};

// VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0:
//
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
//    generated_baseline: VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0,
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
class VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VFMA_A1_111100100d00nnnndddd1100nqm1mmmm_case_0);
};

// VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    advsimd: false,
//    arch: VFPv4,
//    baseline: CondVfpOp,
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
//    generated_baseline: VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0,
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    op: op(6),
//    op1_neg: op(6)=1,
//    pattern: cccc11101d10nnnndddd101snom0mmmm,
//    rule: VFMA_VFMS,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
class VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0
     : public ClassDecoder {
 public:
  VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VFMA_VFMS_cccc11101d10nnnndddd101snom0mmmm_case_0);
};

// VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0:
//
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
//    generated_baseline: VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0,
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
class VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0
     : public ClassDecoder {
 public:
  VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VFMS_A1_111100100d10nnnndddd1100nqm1mmmm_case_0);
};

// VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0:
//
//   {D: D(22),
//    M: M(5),
//    N: N(7),
//    Vd: Vd(15:12),
//    Vm: Vm(3:0),
//    Vn: Vn(19:16),
//    arch: VFPv4,
//    baseline: CondVfpOp,
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
//    generated_baseline: VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0,
//    m: M:Vm
//         if dp_operation
//         else Vm:M,
//    n: N:Vn
//         if dp_operation
//         else Vn:N,
//    op: op(6),
//    op1_neg: op(6)=1,
//    pattern: cccc11101d01nnnndddd101snom0mmmm,
//    rule: VFNMA_VFNMS,
//    safety: [cond(31:28)=1111 => DECODER_ERROR],
//    sz: sz(8),
//    uses: {}}
class VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0
     : public ClassDecoder {
 public:
  VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VFNMA_VFNMS_cccc11101d01nnnndddd101snom0mmmm_case_0);
};

// VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0:
//
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
//    generated_baseline: VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0,
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
class VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VHADD_1111001u0dssnnnndddd0000nqm0mmmm_case_0);
};

// VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0:
//
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
//    generated_baseline: VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0,
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
class VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0
     : public ClassDecoder {
 public:
  VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0()
     : ClassDecoder() {}
  virtual RegisterList defs(Instruction inst) const;
  virtual SafetyLevel safety(Instruction i) const;
  virtual RegisterList uses(Instruction i) const;
 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(
      VHSUB_1111001u0dssnnnndddd0010nqm0mmmm_case_0);
};

} // namespace nacl_arm_test

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_BASELINES_2_H_
