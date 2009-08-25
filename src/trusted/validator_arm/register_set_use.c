/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Models the register set/use of ARM instructions. */

#include "native_client/src/trusted/validator_arm/register_set_use.h"

#define UNCONDITIONAL 14
#define NOT_CONDITION 15

ArmRegisterSet RegisterUses(const NcDecodedInstruction* inst) {
  ArmRegisterSet registers = 0;
  switch(inst->values.cond) {
    case UNCONDITIONAL:
    case NOT_CONDITION:
      break;
    default:
      SetBit(CPSR_INDEX, TRUE, &registers);
      break;
  }
  switch (inst->matched_inst->inst_kind) {
    case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2:
    case ARM_BRANCH_AND_EXCHANGE:
    case ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE:
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    case ARM_ADC:
    case ARM_ADD:
    case ARM_AND:
    case ARM_BIC:
    case ARM_CMN:
    case ARM_CMP:
    case ARM_EOR:
    case ARM_ORR:
    case ARM_RSB:
    case ARM_RSC:
    case ARM_SBC:
    case ARM_SUB:
    case ARM_TEQ:
    case ARM_TST:
      SetBit(inst->values.arg1, TRUE, &registers);
      /* Intentionally fall to the next case. */
    case ARM_MOV:
    case ARM_MVN:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_DATA_IMMEDIATE:
          switch(inst->matched_inst->inst_access->address_submode) {
            case ARM_SUBADDRESSING_LSL:
            case ARM_SUBADDRESSING_LSR:
            case ARM_SUBADDRESSING_ASR:
            case ARM_SUBADDRESSING_ROR:
              SetBit(inst->values.arg4, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        case ARM_DATA_REGISTER:
          switch (inst->matched_inst->inst_access->address_submode) {
            case ARM_SUBADDRESSING_UNDEFINED:
              SetBit(inst->values.arg4, TRUE, &registers);
              break;
            case ARM_SUBADDRESSING_LSL:
            case ARM_SUBADDRESSING_LSR:
            case ARM_SUBADDRESSING_ASR:
            case ARM_SUBADDRESSING_ROR:
              SetBit(inst->values.arg3, TRUE, &registers);
              SetBit(inst->values.arg4, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        case ARM_DATA_RRX:
          SetBit(inst->values.arg4, TRUE, &registers);
          break;
        default:
          break;
      }
      break;
    case ARM_CLZ:
    case ARM_MSR_CPSR_REGISTER:
    case ARM_MSR_SPSR_REGISTER:
    case ARM_REV:
    case ARM_REV16:
    case ARM_REVSH:
    case ARM_SSAT:
    case ARM_SSAT_LSL:
    case ARM_SSAT_ASR:
    case ARM_SSAT16:
    case ARM_SXTB:
    case ARM_SXTB16:
    case ARM_SXTH:
    case ARM_USAT:
    case ARM_USAT_LSL:
    case ARM_USAT_ASR:
    case ARM_USAT16:
    case ARM_UXTB:
    case ARM_UXTB16:
    case ARM_UXTH:
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    case ARM_CPS_FLAGS:
      SetBit(CPSR_INDEX, TRUE, &registers);
      break;
    case ARM_LDC:
    case ARM_LDM_1:
    case ARM_LDM_1_MODIFY:
    case ARM_LDM_2:
    case ARM_LDM_3:
    case ARM_LDREX:
    case ARM_MCRR:
    case ARM_MRRC:
    case ARM_STC:
      SetBit(inst->values.arg1, TRUE, &registers);
      break;
    case ARM_STM_1:
    case ARM_STM_1_MODIFY:
    case ARM_STM_2:
      registers |= inst->values.immediate;
      SetBit(inst->values.arg1, TRUE, &registers);
      break;
    case ARM_STR:
      SetBit(inst->values.arg2, TRUE, &registers);
      /* Intentionally drop to the next case.*/
    case ARM_LDR:
      SetBit(inst->values.arg1, TRUE, &registers);
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_LS_REGISTER:
        case ARM_LS_SCALED_REGISTER:
          SetBit(inst->values.arg4, TRUE, &registers);
          break;
        default:
          break;
      }
      break;
    case ARM_STR_DOUBLEWORD:
    case ARM_STR_HALFWORD:
      SetBit(inst->values.arg2, TRUE, &registers);
      /* Include the implicit register for the second half
       * of the double word, and then treat like single
       * word by intentionally falling to the next case.
       */
      if (inst->values.arg2 + 1 < NUMBER_REGISTERS) {
        SetBit(inst->values.arg2 + 1, TRUE, &registers);
      }
    case ARM_LDR_DOUBLEWORD:
    case ARM_LDR_SIGNED_BYTE:
    case ARM_LDR_SIGNED_HALFWORD:
      SetBit(inst->values.arg1, TRUE, &registers);
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_MLS_REGISTER:
          SetBit(inst->values.arg4, TRUE, &registers);
          break;
        default:
          break;
      }
      break;
    case ARM_SMLAL:
    case ARM_SMLAL_XY:
    case ARM_SMLALD:
    case ARM_SMLALDX:
    case ARM_SMLSLD:
    case ARM_SMLSLDX:
    case ARM_UMAAL:
    case ARM_UMLAL:
      SetBit(inst->values.arg1, TRUE, &registers);
      SetBit(inst->values.arg2, TRUE, &registers);
      SetBit(inst->values.arg3, TRUE, &registers);
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    case ARM_MLA:
    case ARM_SMLA_XY:
    case ARM_SMLAD:
    case ARM_SMLAWB:
    case ARM_SMLAWT:
    case ARM_SMLSD:
    case ARM_SMLSDX:
    case ARM_SMMLA:
    case ARM_SMMLAR:
    case ARM_SMMLS:
    case ARM_SMMLSR:
    case ARM_USADA8:
      SetBit(inst->values.arg2, TRUE, &registers);
      SetBit(inst->values.arg3, TRUE, &registers);
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    case ARM_MUL:
    case ARM_SMMUL:
    case ARM_SMMULR:
    case ARM_SMUAD:
    case ARM_SMUADX:
    case ARM_SMUL_XY:
    case ARM_SMULL:
    case ARM_SMULWB:
    case ARM_SMULWT:
    case ARM_SMUSD:
    case ARM_SMUSDX:
    case ARM_SSUBADDX:
    case ARM_UMULL:
    case ARM_USAD8:
      SetBit(inst->values.arg3, TRUE, &registers);
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    case ARM_SEL:
      SetBit(CPSR_INDEX, TRUE, &registers);
      SetBit(inst->values.arg1, TRUE, &registers);
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    case ARM_PKHBT_1:
    case ARM_PKHBT_1_LSL:
    case ARM_PKHBT_2:
    case ARM_PKHBT_2_ASR:
    case ARM_QADD:
    case ARM_QADD16:
    case ARM_QADD8:
    case ARM_QADDSUBX:
    case ARM_QDADD:
    case ARM_QDSUB:
    case ARM_QSUB:
    case ARM_QSUB16:
    case ARM_QSUB8:
    case ARM_QSUBADDX:
    case ARM_SADD16:
    case ARM_SADD8:
    case ARM_SADDSUBX:
    case ARM_SHADD16:
    case ARM_SHADD8:
    case ARM_SHADDSUBX:
    case ARM_SHSUB16:
    case ARM_SHSUB8:
    case ARM_SHSUBADDX:
    case ARM_SSUB16:
    case ARM_SSUB8:
    case ARM_STREX:
    case ARM_SWP:
    case ARM_SWPB:
    case ARM_SXTAB:
    case ARM_SXTAB16:
    case ARM_SXTAH:
    case ARM_UADD16:
    case ARM_UADD8:
    case ARM_UADDSUBX:
    case ARM_UHADD16:
    case ARM_UHADD8:
    case ARM_UHADDSUBX:
    case ARM_UHSUB16:
    case ARM_UHSUB8:
    case ARM_UHSUBADDX:
    case ARM_UQADD16:
    case ARM_UQADD8:
    case ARM_UQADDSUBX:
    case ARM_UQSUB16:
    case ARM_UQSUB8:
    case ARM_UQSUBADDX:
    case ARM_USUB16:
    case ARM_USUB8:
    case ARM_USUBADDX:
    case ARM_UXTAB:
    case ARM_UXTAB16:
    case ARM_UXTAH:
      SetBit(inst->values.arg1, TRUE, &registers);
      SetBit(inst->values.arg4, TRUE, &registers);
      break;
    default:
      break;
  }
  return registers;
}

ArmRegisterSet RegisterSets(const NcDecodedInstruction* inst) {
  ArmRegisterSet registers = 0;
  switch (inst->matched_inst->inst_kind) {
    case ARM_ADC:
    case ARM_ADD:
    case ARM_AND:
    case ARM_BIC:
    case ARM_EOR:
    case ARM_MOV:
    case ARM_MVN:
    case ARM_ORR:
    case ARM_RSB:
    case ARM_RSC:
    case ARM_SBC:
    case ARM_SUB:
      SetBit(inst->values.arg2, TRUE, &registers);
      /* Intentionally fall to the next case! */
    case ARM_CMN:
    case ARM_CMP:
    case ARM_TEQ:
    case ARM_TST:
      if (GetBit(inst->inst, ARM_S_BIT)) {
        SetBit((PC_INDEX == inst->values.arg2
                ? SPSR_INDEX
                : CPSR_INDEX), TRUE, &registers);
      }
      break;
    case ARM_BRANCH_AND_LINK:
      SetBit(LINK_INDEX, TRUE, &registers);
      SetBit(PC_INDEX, TRUE, &registers);
      break;
    case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_1:
    case ARM_BRANCH_WITH_LINK_AND_EXCHANGE_2:
      SetBit(LINK_INDEX, TRUE, &registers);
      SetBit(CPSR_INDEX, TRUE, &registers);
      SetBit(PC_INDEX, TRUE, &registers);
      break;
    case ARM_BRANCH_AND_EXCHANGE:
    case ARM_BRANCH_AND_EXCHANGE_TO_JAZELLE:
      SetBit(PC_INDEX, TRUE, &registers);
      break;
    case ARM_CLZ:
      SetBit(inst->values.arg2, TRUE, &registers);
      break;
    case ARM_LDC:
    case ARM_STC:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_LSC_IMMEDIATE:
          switch (inst->matched_inst->inst_access->index_mode) {
            case ARM_INDEXING_PRE:
            case ARM_INDEXING_POST:
              SetBit(inst->values.arg1, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    case ARM_LDM_1:
    case ARM_LDM_1_MODIFY:
    case ARM_LDM_2:
    case ARM_LDM_3:
      registers |= inst->values.immediate;
      if (GetBit(inst->inst, ARM_W_BIT)) {
        SetBit(inst->values.arg1, TRUE, &registers);
      }
      break;
    case ARM_STM_1:
    case ARM_STM_1_MODIFY:
    case ARM_STM_2:
      if (GetBit(inst->inst, ARM_W_BIT)) {
        SetBit(inst->values.arg1, TRUE, &registers);
      }
      break;
    case ARM_LDR:
      SetBit(inst->values.arg2, TRUE, &registers);
      /* Intentionally drop to the next case. */
    case ARM_STR:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_LS_IMMEDIATE:
        case ARM_LS_REGISTER:
        case ARM_LS_SCALED_REGISTER:
          switch (inst->matched_inst->inst_access->index_mode) {
            case ARM_INDEXING_PRE:
            case ARM_INDEXING_POST:
              SetBit(inst->values.arg1, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    case ARM_LDR_DOUBLEWORD:
      /* Include the implicit register for the second half
       * of the double word, and then treat like single
       * word by intentionally falling to the next case.
       */
      if (inst->values.arg2 + 1 < NUMBER_REGISTERS) {
        SetBit(inst->values.arg2 + 1, TRUE, &registers);
      }
      /* Intentionally fall to the next case! */
    case ARM_LDR_SIGNED_BYTE:
    case ARM_LDR_SIGNED_HALFWORD:
      SetBit(inst->values.arg2, TRUE, &registers);
      /* Intentionally drop to the next case. */
    case ARM_STR_HALFWORD:
    case ARM_STR_DOUBLEWORD:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_MLS_IMMEDIATE:
        case ARM_MLS_REGISTER:
          switch (inst->matched_inst->inst_access->index_mode) {
            case ARM_INDEXING_PRE:
            case ARM_INDEXING_POST:
              SetBit(inst->values.arg1, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    case ARM_QADD:
    case ARM_QDADD:
    case ARM_QDSUB:
    case ARM_QSUB:
    case ARM_SADD16:
    case ARM_SADD8:
    case ARM_QADDSUBX:
    case ARM_SSAT:
    case ARM_SSAT_LSL:
    case ARM_SSAT_ASR:
    case ARM_SSAT16:
    case ARM_SSUB16:
    case ARM_SSUB8:
    case ARM_SSUBADDX:
    case ARM_UADD16:
    case ARM_UADD8:
    case ARM_UADDSUBX:
    case ARM_USAT:
    case ARM_USAT_LSL:
    case ARM_USAT_ASR:
    case ARM_USAT16:
    case ARM_USUB16:
    case ARM_USUB8:
    case ARM_USUBADDX:
      SetBit(CPSR_INDEX, TRUE, &registers);
      SetBit(inst->values.arg2, TRUE, &registers);
      break;
    case ARM_LDREX:
    case ARM_MCR:
    case ARM_MCRR:
    case ARM_MRC:
    case ARM_MRRC:
    case ARM_MRS_CPSR:
    case ARM_MRS_SPSR:
    case ARM_PKHBT_1:
    case ARM_PKHBT_1_LSL:
    case ARM_PKHBT_2:
    case ARM_PKHBT_2_ASR:
    case ARM_QADD16:
    case ARM_QADD8:
    case ARM_QSUB16:
    case ARM_QSUB8:
    case ARM_QSUBADDX:
    case ARM_REV:
    case ARM_REV16:
    case ARM_REVSH:
    case ARM_SADDSUBX:
    case ARM_SEL:
    case ARM_SHADD16:
    case ARM_SHADD8:
    case ARM_SHADDSUBX:
    case ARM_SHSUB16:
    case ARM_SHSUB8:
    case ARM_SHSUBADDX:
    case ARM_STREX:
    case ARM_SWP:
    case ARM_SWPB:
    case ARM_SXTAB:
    case ARM_SXTAB16:
    case ARM_SXTAH:
    case ARM_SXTB:
    case ARM_SXTB16:
    case ARM_SXTH:
    case ARM_UHADD16:
    case ARM_UHADD8:
    case ARM_UHADDSUBX:
    case ARM_UHSUB16:
    case ARM_UHSUB8:
    case ARM_UHSUBADDX:
    case ARM_UQADD16:
    case ARM_UQADD8:
    case ARM_UQADDSUBX:
    case ARM_UQSUB16:
    case ARM_UQSUB8:
    case ARM_UQSUBADDX:
    case ARM_UXTAB:
    case ARM_UXTAB16:
    case ARM_UXTAH:
    case ARM_UXTB:
    case ARM_UXTB16:
    case ARM_UXTH:
      SetBit(inst->values.arg2, TRUE, &registers);
      break;
    case ARM_SMLAL_XY:
    case ARM_SMLALD:
    case ARM_SMLALDX:
    case ARM_SMLSLD:
    case ARM_SMLSLDX:
    case ARM_UMAAL:
      SetBit(inst->values.arg1, TRUE, &registers);
      SetBit(inst->values.arg2, TRUE, &registers);
      break;
    case ARM_SMLAL:
    case ARM_SMULL:
    case ARM_UMLAL:
    case ARM_UMULL:
      if (GetBit(inst->inst, ARM_S_BIT)) {
        SetBit(CPSR_INDEX, TRUE, &registers);
      }
      SetBit(inst->values.arg1, TRUE, &registers);
      SetBit(inst->values.arg2, TRUE, &registers);
      break;
    case ARM_MLA:
    case ARM_MUL:
    case ARM_SMLAWB:
    case ARM_SMLAWT:
    case ARM_SMUAD:
    case ARM_SMUADX:
      if (GetBit(inst->inst, ARM_S_BIT)) {
        SetBit(CPSR_INDEX, TRUE, &registers);
      }
      SetBit(inst->values.arg1, TRUE, &registers);
      break;
    case ARM_SMLA_XY:
    case ARM_SMLAD:
    case ARM_SMLSD:
    case ARM_SMLSDX:
      SetBit(CPSR_INDEX, TRUE, &registers);
      SetBit(inst->values.arg1, TRUE, &registers);
      break;
    case ARM_SMMLA:
    case ARM_SMMLAR:
    case ARM_SMMLS:
    case ARM_SMMLSR:
    case ARM_SMMUL:
    case ARM_SMMULR:
    case ARM_SMUL_XY:
    case ARM_SMULWB:
    case ARM_SMULWT:
    case ARM_SMUSD:
    case ARM_SMUSDX:
    case ARM_USAD8:
    case ARM_USADA8:
      SetBit(inst->values.arg1, TRUE, &registers);
      break;
    case ARM_MSR_CPSR_IMMEDIATE:
    case ARM_MSR_CPSR_REGISTER:
    case ARM_SETEND_BE:
    case ARM_SETEND_LE:
      SetBit(CPSR_INDEX, TRUE, &registers);
      break;
    case ARM_MSR_SPSR_IMMEDIATE:
    case ARM_MSR_SPSR_REGISTER:
      SetBit(SPSR_INDEX, TRUE, &registers);
      break;
    default:
      break;
  }
  return registers;
}

ArmRegisterSet RegisterSetIncremented(const NcDecodedInstruction* inst) {
  ArmRegisterSet registers = 0;
  switch (inst->matched_inst->inst_kind) {
    case ARM_LDC:
    case ARM_STC:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_LSC_IMMEDIATE:
          switch (inst->matched_inst->inst_access->index_mode) {
            case ARM_INDEXING_PRE:
            case ARM_INDEXING_POST:
              SetBit(inst->values.arg1, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    case ARM_LDM_1:
    case ARM_LDM_1_MODIFY:
    case ARM_LDM_2:
    case ARM_LDM_3:
    case ARM_STM_1:
    case ARM_STM_1_MODIFY:
    case ARM_STM_2:
      if (GetBit(inst->inst, ARM_W_BIT)) {
        SetBit(inst->values.arg1, TRUE, &registers);
      }
      break;
    case ARM_LDR:
    case ARM_STR:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_LS_IMMEDIATE:
        case ARM_LS_REGISTER:
        case ARM_LS_SCALED_REGISTER:
          switch (inst->matched_inst->inst_access->index_mode) {
            case ARM_INDEXING_PRE:
            case ARM_INDEXING_POST:
              SetBit(inst->values.arg1, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    case ARM_LDR_DOUBLEWORD:
    case ARM_STR_DOUBLEWORD:
    case ARM_LDR_SIGNED_BYTE:
    case ARM_LDR_SIGNED_HALFWORD:
    case ARM_STR_HALFWORD:
      switch (inst->matched_inst->inst_access->address_mode) {
        case ARM_MLS_IMMEDIATE:
        case ARM_MLS_REGISTER:
          switch (inst->matched_inst->inst_access->index_mode) {
            case ARM_INDEXING_PRE:
            case ARM_INDEXING_POST:
              SetBit(inst->values.arg1, TRUE, &registers);
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  return registers;
}
