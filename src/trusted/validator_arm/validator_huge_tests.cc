/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

/*
 * Huge unit tests for the ARM validator
 *
 * See validator_tests.cc for the short tests,
 * and validator_tests.h for the testing infrastructure.
 */

#include "native_client/src/trusted/validator_arm/validator_tests.h"

using nacl_val_arm_test::ProblemRecord;
using nacl_val_arm_test::ProblemSpy;
using nacl_val_arm_test::ValidatorTests;
using nacl_val_arm_test::kDefaultBaseAddr;
using nacl_val_arm_test::kCodeRegionSize;
using nacl_val_arm_test::kDataRegionSize;
using nacl_val_arm_test::kAbiReadOnlyRegisters;
using nacl_val_arm_test::kAbiDataAddrRegisters;
using nacl_val_arm_test::arm_inst;

namespace {

uint32_t ARMExpandImm(uint32_t imm12) {
  uint32_t unrotated_value = imm12 & 0xFF;
  uint32_t ror_amount = ((imm12 >> 8) & 0xF) << 1;
  return (ror_amount == 0) ?
      unrotated_value :
      ((unrotated_value >> ror_amount) |
       (unrotated_value << (32 - ror_amount)));
}

TEST_F(ValidatorTests, WholeA32InstructionSpaceTesting) {
  // Go through all 2**32 ARM instruction encodings, and validate individual
  // instruction's properties.

  // TODO(jfb) We're currently only testing one possible code address
  //           mask, and we're not checking whether the validator
  //           restricts the values to the ones we should be testing.
  const uint32_t code_address_mask = _validator.code_address_mask();
  EXPECT_NE(code_address_mask, 0u);  // There should be some mask.

  const uint32_t data_address_mask = _validator.data_address_mask();
  EXPECT_NE(data_address_mask, 0u);  // There should be some mask.
  EXPECT_EQ(nacl::PopCount(~data_address_mask + 1) , 1);  // Power of 2.
  const uint32_t data_address_mask_msb = 31;  // Implied by the above.
  const uint32_t min_data_address_mask_lsb =
      nacl::CountTrailingZeroes(data_address_mask);

  const nacl_arm_dec::Arm32DecoderState decode_state;
  uint32_t i = 0;
  const uint32_t last_i = std::numeric_limits<uint32_t>::max();
  do {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&i);
    CodeSegment segment(bytes, kDefaultBaseAddr, sizeof(arm_inst));
    nacl_arm_val::DecodedInstruction inst(kDefaultBaseAddr,
                                          segment[kDefaultBaseAddr],
                                          decode_state.decode(
                                              segment[kDefaultBaseAddr]));

    // Information obtained from the decoder itself.
    bool defs_pc = inst.defs().Contains(nacl_arm_dec::kRegisterPc);
    bool is_may_be_safe = inst.safety() == nacl_arm_dec::MAY_BE_SAFE;
    bool is_relative_branch = inst.is_relative_branch();
    bool is_indirect_branch = !inst.branch_target_register()
        .Equals(nacl_arm_dec::kRegisterNone);
    bool is_branch = is_relative_branch || is_indirect_branch;
    uint32_t branch_target = inst.branch_target();
    uint32_t branch_target_register = inst.branch_target_register().number();
    bool is_literal_pool_head = inst.is_literal_pool_head();
    uint32_t base_address_register = inst.base_address_register().number();
    bool has_base_address_register = !inst.base_address_register()
        .Equals(nacl_arm_dec::kRegisterNone);
    bool is_literal_load = inst.is_literal_load();
    bool clears_code_bits = inst.clears_bits(code_address_mask);
    bool clears_data_bits = inst.clears_bits(data_address_mask);
    uint32_t sets_Z_if_data_bits_clear_register_bitmask = 0;
    for (int reg = 0; reg < 16; ++reg) {
      // Check the property for every register value.
      nacl_arm_dec::Register r(reg);
      sets_Z_if_data_bits_clear_register_bitmask |=
          ((int)inst.sets_Z_if_bits_clear(r, data_address_mask)) << reg;
    }

    // Information obtained independently of the decoder.
    bool expect_unconditional = ((i & 0xF0000000) == 0xF0000000);
    bool expect_b_or_bl = !expect_unconditional &&
        // cccc 101L iiii iiii iiii iiii iiii iiii
        ((i & 0x0E000000) == 0x0A000000);
    bool expect_bx_or_blx = !expect_unconditional &&
        // cccc 0001 0010 1111 1111 1111 00L1 mmmm
        ((i & 0x0FFFFFD0) == 0x012FFF10);
    uint32_t expected_branch_target = !expect_b_or_bl ?
        kDefaultBaseAddr :
        // imm32 = SignExtend(imm24:'00', 32);
        // PC reads as the address of the current instruction plus 8.
        (kDefaultBaseAddr + 8 + (((int32_t)i << 8) >> 6));
    uint32_t expected_branch_target_register = expect_bx_or_blx ?
        (i & 0xF) :  // When present, always Rm(3:0).
        nacl_arm_dec::kRegisterNone.number();
    bool expect_literal_pool_head =
        (i == nacl_arm_dec::kLiteralPoolHeadInstruction);
    bool expect_load_store_or_unsafe = expect_unconditional ?
        // Advanced SIMD element or structure load/store instructions.
        // 1111 0100 xx0x xxxx xxxx xxxx xxxx xxxx
        ((i & 0x0F100000) == 0x04000000) :
        (  // Conditional instructions:
            // Synchronization primitives.
            // cccc 0001 xxxx xxxx xxxx xxxx 1001 xxxx
            ((i & 0x0F0000F0) == 0x01000090) ||
            // Extra load/store instructions[, unprivileged].
            // cccc 000x xxxx xxxx xxxx xxxx 1011 xxxx
            // cccc 000x xxxx xxxx xxxx xxxx 11x1 xxxx
            ((i & 0x0E0000F0) == 0x000000B0) ||
            ((i & 0x0E0000D0) == 0x000000D0) ||
            // Load/store word and unsigned bytes.
            // cccc 01Ax xxxx xxxx xxxx xxxx xxxB xxxx
            // With either A==0 or B==0.
            ((i & 0x0E000000) == 0x04000000) ||
            ((i & 0x0E000010) == 0x06000000) ||
            // Branch, branch with link, and block data transfer.
            // cccc 100x xxxx xxxx xxxx xxxx xxxx xxxx
            ((i & 0x0E000000) == 0x08000000) ||
            // Extension register load/store instructions.
            // cccc 110x xxxx nnnn xxxx 101x xxxx xxxx
            // Except opcode(24:20)=0010x, which is 64-bit transfers
            // between ARM core and extension registers.
            (((i & 0x0E000E00) == 0x0C000A00) &&
             ((i & 0x01E00000) != 0x00400000)));
    uint32_t expected_base_address_register = expect_load_store_or_unsafe ?
        (i >> 16) & 0xF :  // When present, always Rn(19:16).
        nacl_arm_dec::kRegisterNone.number();
    bool expect_literal_load_or_unsafe = !expect_unconditional &&
        (
            // Extra load/store instructions.
            // LDRH:  cccc 0001 U101 1111 tttt iiii 1011 iiii
            // LDRSB: cccc 0001 U101 1111 tttt iiii 1101 iiii
            // LDRSH: cccc 0001 U101 1111 tttt iiii 1111 iiii
            (((i & 0x0F7F0090) == 0x015F0090) &&
             ((i & 0x00000060) != 0x00000000)) ||
            // LDRD:  cccc 0001 U100 1111 tttt iiii 1101 iiii
            (((i & 0x0F7F00F0) == 0x014F00D0)) ||
            // Load/store word and unsigned bytes.
            // LDR:   cccc 0101 U001 1111 tttt iiii iiii iiii
            // LDRB:  cccc 0101 U101 1111 tttt iiii iiii iiii
            (((i & 0x0F3F0000) == 0x051F0000)) ||
            // Extension register load/store instructions.
            // VLDM:  cccc 1100 1x01 1111 xxxx 101x xxxx xxxx
            // VLDM:  cccc 1100 1x11 1111 xxxx 101x xxxx xxxx
            // VLDR:  cccc 1101 xx01 1111 xxxx 101x xxxx xxxx
            // VLDM:  cccc 1101 0x11 1111 xxxx 101x xxxx xxxx
            (((i & 0x0F9F0E00) == 0x0C9F0A00) ||
             ((i & 0x0F3F0E00) == 0x0D1F0A00) ||
             ((i & 0x0FBF0E00) == 0x0D3F0A00)));
    bool expect_clears_code_bits_or_unsafe = !expect_unconditional &&
        ((  // BIC{S} Rd, Rn, #imm.
            // cccc 0011 110S nnnn dddd iiii iiii iiii
            ((i & 0x0FE00000) == 0x03C00000) &&
            ((ARMExpandImm(i & 0xFFF) & code_address_mask) ==
             code_address_mask)) ||
         (  // BFC Rd, #0, #32  // Clears all the bits.
             // cccc 0111 1101 1111 dddd 0000 0001 1111
             ((i & 0x0FFF0FFF) == 0x07DF001F)));
    bool expect_clears_data_bits_or_unsafe = !expect_unconditional &&
        ((  // BIC{S} Rd, Rn, #imm.
            // cccc 0011 110S nnnn dddd iiii iiii iiii
            ((i & 0x0FE00000) == 0x03C00000) &&
            ((ARMExpandImm(i & 0xFFF) & data_address_mask) ==
             data_address_mask)) ||
         (  // BFC Rd, #lsb, #width.
             // cccc 0111 110M MMMM dddd LLLL L001 1111
             ((i & 0x0FE0007F) == 0x07C0001F) &&
             (((i >> 7) & 0x1F) <= min_data_address_mask_lsb) &&
             (((i >> 16) & 0x1F) == data_address_mask_msb)));
    uint32_t expect_sets_Z_if_data_bits_clear_register_bitmask =
        !expect_unconditional &&
        // TST Rn, #imm
        // cccc 0011 0001 nnnn 0000 iiii iiii iiii
        (((i & 0x0FF0F000) == 0x03100000) &&
         ((ARMExpandImm(i & 0xFFF) & data_address_mask) ==
          data_address_mask)) ?
        (1 << ((i >> 16) & 0xF)) :  // Set bit corresponding to Rn.
        0;

    // Validate that every single method in DecodedInstruction returns the
    // expected value.

    // TODO(jfb) Validate safety.
    // TODO(jfb) Validate defs.

    // B and BL are relative branches, others aren't.
    EXPECT_EQ(is_relative_branch, expect_b_or_bl);
    // BX and BLX are indirect branches, others aren't.
    EXPECT_EQ(is_indirect_branch, expect_bx_or_blx);
    // B and BL's target is calculated properly.
    EXPECT_EQ(branch_target, expected_branch_target);
    // BX and BLX's target register is correct.
    EXPECT_EQ(branch_target_register, expected_branch_target_register);
    // Only BKPT #special_imm is a literal pool head.
    EXPECT_EQ(is_literal_pool_head, expect_literal_pool_head);
    if (is_may_be_safe) {
      // TODO(jfb) The following tests aren't full negative test: some
      //           instructions might be left out by being marked as
      //           unsafe. We need to explicitly list all encodings that
      //           are expected to be unsafe for load/store, and bit
      //           clearing instructions.
      //           Fixing this also requires Advanced SIMD support
      //           in the decoder.

      // The ARM load/stores instruction space is either unsafe (e.g. undefined,
      // forbidden, ...), or is an actual load/store and has the right base
      // address register. Other instructions (non-load/store) don't have a base
      // address register.
      EXPECT_EQ(has_base_address_register, expect_load_store_or_unsafe);
      EXPECT_EQ(base_address_register, expected_base_address_register);
      // Literal loads are properly decoded as such.
      EXPECT_EQ(is_literal_load, expect_literal_load_or_unsafe);
      // SFI data and code bit clearing is detected properly.
      EXPECT_EQ(clears_code_bits, expect_clears_code_bits_or_unsafe);
      EXPECT_EQ(clears_data_bits, expect_clears_data_bits_or_unsafe);
    }
    EXPECT_EQ(sets_Z_if_data_bits_clear_register_bitmask,
              expect_sets_Z_if_data_bits_clear_register_bitmask);

    // TODO(jfb) Validate immediate_addressing_defs.
    // TODO(jfb) Validate defines.
    // TODO(jfb) Validate defines_any.
    // TODO(jfb) Validate defines_all.

    EXPECT_EQ(inst.inst().Bits(), i);

    // Validate that DecodedInstruction returns values that are consistent
    // with ARM rules as well as with NaCl's SFI rules.

    // Only branches and instructions that clear the SFI code bits
    // can write to PC.
    EXPECT_FALSE(defs_pc && is_may_be_safe && !is_branch && !clears_code_bits);
    // All branches must implicitly define PC.
    EXPECT_FALSE(is_branch && !defs_pc);
  } while (i++ != last_i);
}

}  // anonymous namespace

// Test driver function.
int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
