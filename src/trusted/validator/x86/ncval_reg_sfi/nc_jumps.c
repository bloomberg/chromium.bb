/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * nc_jumps.c - Validate where valid jumps can occur.
 */

#include <assert.h>
#include <stdlib.h>

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_jumps.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_trans.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/address_sets.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

#include "native_client/src/trusted/validator/x86/decoder/ncop_exps_inl.c"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter_inl.c"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/address_sets_inl.c"

Bool NACL_FLAGS_identity_mask = FALSE;

static INLINE uint8_t NaClGetJumpMask(NaClValidatorState* vstate) {
  return NACL_FLAGS_identity_mask
      ? (uint8_t) 0xFF
      : (uint8_t) (~vstate->alignment_mask);
}

/* Generates a jump validator. */
Bool NaClJumpValidatorInitialize(NaClValidatorState* vstate) {
  NaClJumpSets* jump_sets = &vstate->jump_sets;
  jump_sets->actual_targets = NaClAddressSetCreate(vstate->codesize);
  jump_sets->possible_targets = NaClAddressSetCreate(vstate->codesize);
  jump_sets->removed_targets = NaClAddressSetCreate(vstate->codesize);
  if (jump_sets->actual_targets == NULL ||
      jump_sets->possible_targets == NULL ||
      jump_sets->removed_targets == NULL) {
    NaClValidatorMessage(LOG_ERROR, vstate, "unable to allocate jump sets");
    NaClJumpValidatorCleanUp(vstate);
    return FALSE;
  }
  jump_sets->set_array_size = NaClAddressSetArraySize(vstate->codesize);
  return TRUE;
}

/* Record that there is an explicit jump from the from_address to the
 * to_address, for the validation defined by the validator state.
 * Parameters:
 *   vstate - The state of the validator.
 *   from_address - The address of the instruction that does the jump.
 *   to_address - The address that the instruction jumps to.
 */
static void NaClAddJumpToJumpSets(NaClValidatorState* vstate,
                                  NaClPcAddress from_address,
                                  NaClPcAddress to_address) {
  /* If the address is in the code segment, assume good (unless we later find it
   * jumping into a pseudo instruction). Otherwise, only allow if 0 mod 32.
   */
  DEBUG(NaClLog(LOG_INFO, "Add jump to jump sets: %"
                NACL_PRIxNaClPcAddress" -> %"NACL_PRIxNaClPcAddress"\n",
                from_address, to_address));
  if (vstate->vbase <= to_address
      && to_address < vstate->vbase + vstate->codesize) {
    /* Remember address for checking later. */
    DEBUG(NaClLog(LOG_INFO, "Add jump to target: %"NACL_PRIxNaClPcAddress
                  " -> %"NACL_PRIxNaClPcAddress"\n",
                  from_address, to_address));
    NaClAddressSetAddInline(vstate->jump_sets.actual_targets,
                            to_address, vstate);
  } else if ((to_address & vstate->alignment_mask) == 0) {
    /* Allow bundle-aligned jump.  If the jump overflows or underflows the
     * 4GB untrusted address space it will hit the guard regions.  The largest
     * potential jump offset is +/-2GB.  We could allow direct jumps only within
     * the 4GB untrusted address space, but there is no need for this
     * restriction and it would make validation judgements position-dependent.
     */
  } else {
    NaClValidatorInstMessage(LOG_ERROR, vstate, vstate->cur_inst_state,
                             "Instruction jumps to bad address\n");
  }
}

static Bool NaClExtractBinaryOperandIndices(
    NaClInstState* inst,
    int* op_1,
    int* op_2) {
  uint32_t index;
  NaClExpVector* nodes = NaClInstStateExpVector(inst);
  *op_1 = -1;
  *op_2 = -1;

  for (index = 0; index < nodes->number_expr_nodes; ++index) {
    if (OperandReference == nodes->node[index].kind) {
      if (-1 == *op_1) {
        *op_1 = index + 1;
      } else {
        *op_2 = index + 1;
        return TRUE;
      }
    }
  }
  return FALSE;
}

/* Given the index of a memory offset in the given expression vector, returns
 * index iff the memory offset is of the form [base+index].
 *
 * Parameters:
 *   vector - The expression vector for the instruction being examined.
 *   memoffset_index - The index into the expression vector where the memory
 *            offset occurs.
 *   base_register - The base register expected in the memory offset.
 * Returns:
 *   index if of correct form, RegUnknown otherwise.
 */
static NaClOpKind NaClMemOffsetMatchesBasePlusIndex(
    NaClExpVector* vector,
    int memoffset_index,
    NaClOpKind base_register) {
  int r1_index = memoffset_index + 1;
  int r2_index = r1_index + NaClExpWidth(vector, r1_index);
  int scale_index = r2_index + NaClExpWidth(vector, r2_index);
  int disp_index = scale_index + NaClExpWidth(vector, scale_index);
  NaClOpKind r1 = NaClGetExpVectorRegister(vector, r1_index);
  NaClOpKind r2 = NaClGetExpVectorRegister(vector, r2_index);
  uint64_t scale = NaClGetExpConstant(vector, scale_index);
  uint64_t disp = NaClGetExpConstant(vector, disp_index);
  assert(ExprMemOffset == vector->node[memoffset_index].kind);
  return (r1 == base_register &&
          1 == scale &&
          0 == disp) ? r2 : RegUnknown;
}

/* Returns the 32-bit register for instructions of the form
 *
 *    and %reg32, MASK
 *
 * where MASK is all 1/s except for the alignment mask bits, which must be zero.
 *
 * It is assumed that opcode 0x83 is used for the AND operation, and hence, the
 * mask is a single byte.
 *
 * Returns RegUnknown if the instruction doesn't match the form listed above.
 */
static NaClOpKind NaClGetAndMaskReg32(NaClValidatorState* vstate,
                                      size_t distance) {
  NaClInstState* state;
  const NaClInst* inst;
  int op_1, op_2;
  NaClExpVector* nodes;
  NaClExp* node;
  uint8_t mask;
  NaClOpKind reg32;
  NaClInstIter* iter = vstate->cur_iter;

  /* Get the corresponding and instruction. */
  if (!NaClInstIterHasLookbackStateInline(iter, distance)) return RegUnknown;
  state = NaClInstIterGetLookbackStateInline(iter, distance);
  inst = NaClInstStateInst(state);
  if ((InstAnd != inst->name) ||
      (state->num_opcode_bytes == 0) ||
      (0x83 != state->bytes.byte[state->num_prefix_bytes])) return RegUnknown;
  DEBUG(NaClLog(LOG_INFO, "inst(%d): and mask: ", (int) distance);
        NaClInstStateInstPrint(NaClLogGetGio(), state));

  /* Extract the values of the two operands for the and. */
  if (!NaClExtractBinaryOperandIndices(state, &op_1, &op_2)) return RegUnknown;

  /* Extract the destination register of the and. */
  nodes = NaClInstStateExpVector(state);
  node = &nodes->node[op_1];
  if (ExprRegister != node->kind) return RegUnknown;

  reg32 = NaClGetExpRegisterInline(node);
  DEBUG(NaClLog(LOG_INFO, "and mask reg = %s\n", NaClOpKindName(reg32)));

  /* Check that the mask is ok. */
  mask = NaClGetJumpMask(vstate);
  DEBUG(NaClLog(LOG_INFO, "mask = %"NACL_PRIx8"\n", mask));

  assert(0xf0 == mask || 0xe0 == mask); /* alignment must be either 16 or 32. */
  node = &nodes->node[op_2];
  if (ExprConstant != node->kind || mask != node->value) return RegUnknown;
  DEBUG(NaClLog(LOG_INFO, "is mask constant\n"));

  return reg32;
}

/* Returns the 64-bit index register reg_a of an lea instruction of the form
 *
 *    lea %reg_b, [%RBASE + %reg_a]
 *
 * given the 64-bit register reg_b. Returns RegUnknown if the lea doesn't
 * match the maksing form needed.
 */
static NaClOpKind NaClGetLeaMask64(NaClValidatorState* vstate,
                                   size_t distance,
                                   NaClOpKind reg_b) {
  NaClInstState* state;
  const NaClInst* inst;
  int op_1, op_2;
  NaClExpVector* nodes;
  NaClExp* node;
  NaClInstIter* iter = vstate->cur_iter;

  /* Get the corresponding lea instruction. */
  if (!NaClInstIterHasLookbackStateInline(iter, distance)) return RegUnknown;
  state = NaClInstIterGetLookbackStateInline(iter, distance);
  inst = NaClInstStateInst(state);
  if (InstLea != inst->name) return RegUnknown;
  DEBUG(NaClLog(LOG_INFO, "inst(%d): lea mask: ", (int) distance);
        NaClInstStateInstPrint(NaClLogGetGio(), state));

  /* Extract the values of the two operands for the lea instruction. */
  if (!NaClExtractBinaryOperandIndices(state, &op_1, &op_2)) return RegUnknown;
  DEBUG(NaClLog(LOG_INFO, "op_1 index = %d, op_2 index = %d\n", op_1, op_2));

  /* Extract the destination register of the lea/and, and verify that
   * it is the register reg_b
   */
  nodes = NaClInstStateExpVector(state);
  node = &nodes->node[op_1];
  if (ExprRegister != node->kind) return RegUnknown;
  if (reg_b != NaClGetExpRegisterInline(node)) return RegUnknown;

  /* Check that we have [%RBASE + %reg_a] as second argument to lea */
  node = &nodes->node[op_2];
  if (ExprMemOffset != node->kind) return RegUnknown;
  return NaClMemOffsetMatchesBasePlusIndex(nodes, op_2, vstate->base_register);
}

/* Returns true if the 64-bit register reg64 set by an instruction of the form
 *
 *    add %reg64 %RBASE
 *
 * The instruction checked is the "distance" instruction from the current
 * instruction being looked at by the specified iterator.
 */
static Bool NaClIsAddRbaseToReg64(NaClValidatorState* vstate,
                                  size_t distance,
                                  NaClOpKind reg64) {
  NaClInstState* state;
  const NaClInst* inst;
  int op_1, op_2;
  NaClExpVector* nodes;
  NaClExp* node;
  NaClOpKind reg;
  NaClInstIter* iter = vstate->cur_iter;

  /* Get the corresponding instruction. */
  if (!NaClInstIterHasLookbackStateInline(iter, distance)) return FALSE;
  state = NaClInstIterGetLookbackStateInline(iter, distance);
  inst = NaClInstStateInst(state);
  if (InstAdd != inst->name) return FALSE;
  DEBUG(NaClLog(LOG_INFO, "inst(%d): add rbase: ", (int) distance);
        NaClInstStateInstPrint(NaClLogGetGio(), state));

  /* Extract the values of the two operands for the and. */
  if (!NaClExtractBinaryOperandIndices(state, &op_1, &op_2)) return FALSE;

  /* Extract the destination register of the and. */
  nodes = NaClInstStateExpVector(state);
  node = &nodes->node[op_1];
  if (ExprRegister != node->kind) return FALSE;

  /* Check that destination register matches wanted register. */
  reg = NaClGetExpRegisterInline(node);
  if (reg != reg64) return FALSE;

  /* Check that source register is the base register. */
  return NaClGetExpVectorRegister(nodes, op_2) == vstate->base_register;
}

/* Checks if an indirect jump (in 64-bit mode) is native client compliant.
 *
 * Expects pattern:
 *    and %REG32-A, MASK
 *    lea %REG64-B, [%RBASE + %REG64-A]
 *    jmp %REG64-B
 *
 * Or:
 *    and %REG32-A, MASK
 *    add %REG64-A, %RBASE
 *    jmp %REG64-A
 *
 * where MASK is all 1/s except for the alignment mask bits, which must be zero.
 *
 * REG32 is the corresponding 32-bit register that whose value will get zero
 * extended by the AND operation into the corresponding 64-bit register REG64.
 *
 * It is assumed that opcode 0x83 is used for the AND operation, and hence, the
 * mask is a single byte.
 *
 * Note: applies to all kinds of jumps and calls.
 *
 * Parameters:
 *   vstate - The state of the validator.
 *   reg - The register used in the jump instruction.
 */
static void NaClAddRegisterJumpIndirect64(NaClValidatorState* vstate,
                                          NaClExp* reg) {
  NaClOpKind jump_reg, and_reg32, and_reg64;
  DEBUG(NaClLog(LOG_INFO, "checking indirect jump: ");
        NaClInstStateInstPrint(NaClLogGetGio(), vstate->cur_inst_state);
        gprintf(NaClLogGetGio(), "jump_reg = %s\n", NaClOpKindName(jump_reg)));

  /* Do the following block exactly once. Use loop so that "break" can
   * be used for premature exit of block.
   */
  do {
    /* Check that jump register is 64-bit. */
    if (!NaClHasBit(reg->flags, NACL_EFLAG(ExprSize64))) return;
    jump_reg = NaClGetExpRegisterInline(reg);
    if (RegUnknown == jump_reg) return;

    /* Check that sequence begins with an appropriate and instruction. */
    and_reg32 = NaClGetAndMaskReg32(vstate, 2);
    if (RegUnknown == and_reg32) break;

    /* Get corresponding 64-bit register for 32-bit result of and. */
    and_reg64 = NaClGet64For32BitReg(and_reg32);
    if (RegUnknown == and_reg64) break;

    /* Check that the middle instruction is either an appropriate lea or
     * add instruction.
     */
    if ((and_reg64 != NaClGetLeaMask64(vstate, 1, jump_reg)) &&
        !NaClIsAddRbaseToReg64(vstate, 1, and_reg64)) break;

    /* If reached, indirect jump is properly masked. */
    DEBUG(NaClLog(LOG_INFO, "Protect indirect jump instructions\n"));
    NaClMarkInstructionJumpIllegal(
        vstate, NaClInstIterGetLookbackStateInline(vstate->cur_iter, 1));
    NaClMarkInstructionJumpIllegal(vstate, vstate->cur_inst_state);
    return;
  } while(0);

  /* If reached, mask was not found. */
  NaClValidatorInstMessage(LOG_ERROR, vstate, vstate->cur_inst_state,
                           "Invalid indirect jump\n");
}

/* Checks if an indirect jump (in 32-bit mode) is native client compliant.
 *
 * Expects pattern:
 *    and %REG, MASK
 *    jmp %REG
 *
 * where the MASK is all 1's except for the alignment mask bits, which must
 * be zero.
 *
 * It is assumed that opcode 0x83 is used for the AND operation, and hence, the
 * mask is a single byte.
 *
 * Note: applies to all kinds of jumps and calls.
 *
 * Parameters:
 *   state - The state of the validator.
 *   reg - The register used in the jump instruction.
 */
static void NaClAddRegisterJumpIndirect32(NaClValidatorState* vstate,
                                          NaClExp* reg) {
  NaClOpKind jump_reg, and_reg;
  DEBUG(NaClLog(LOG_INFO, "checking indirect jump: ");
        NaClInstStateInstPrint(NaClLogGetGio(), vstate->cur_inst_state);
        gprintf(NaClLogGetGio(), "jump_reg = %s\n", NaClOpKindName(jump_reg)));

  /* Do the following block exactly once. Use loop so that "break" can
   * be used for premature exit of block.
   */
  do {
    /* Check that jump register is 32-bit. */
    if (!NaClHasBit(reg->flags, NACL_EFLAG(ExprSize32))) return;
    jump_reg = NaClGetExpRegisterInline(reg);
    if (RegUnknown == jump_reg) return;

    /* Check that sequence begins with an appropriate and instruction. */
    and_reg = NaClGetAndMaskReg32(vstate, 1);
    if (jump_reg != and_reg) break;

    /* If reached, indirect jump is properly masked. */
    DEBUG(NaClLog(LOG_INFO, "Protect register jump indirect\n"));
    NaClMarkInstructionJumpIllegal(vstate, vstate->cur_inst_state);
    return;
  } while(0);

  /* If reached, mask was not found. */
  NaClValidatorInstMessage(LOG_ERROR, vstate, vstate->cur_inst_state,
                           "Invalid indirect jump\n");
}

/* Given a jump statement, add the corresponding (explicit) jump value
 * to the set of actual jump targets.
 * Parameters:
 *   vstate - The state of the validator.
 */
static void NaClAddExprJumpTarget(NaClValidatorState* vstate) {
  uint32_t i;
  NaClInstState* inst_state = vstate->cur_inst_state;
  NaClExpVector* vector = vstate->cur_inst_vector;
  DEBUG(NaClLog(LOG_INFO, "jump checking: ");
        NaClInstStateInstPrint(NaClLogGetGio(), inst_state));
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    if (!NaClHasBit(vector->node[i].flags, NACL_EFLAG(ExprJumpTarget)))
      continue;
    switch (vector->node[i].kind) {
      case ExprRegister:
        if (64 == NACL_TARGET_SUBARCH) {
          NaClAddRegisterJumpIndirect64(vstate,
                                        &vector->node[i]);
        } else {
          NaClAddRegisterJumpIndirect32(vstate,
                                        &vector->node[i]);
        }
        break;
      case ExprConstant:
      case ExprConstant64:
        /* Direct jump. */
        NaClAddJumpToJumpSets(vstate, NaClInstStateVpc(inst_state),
                              (NaClPcAddress) NaClGetExpConstant(vector, i));
        break;
      default:
        NaClValidatorInstMessage(
            LOG_ERROR, vstate, inst_state,
            "Jump not native client compliant\n");
    }
  }
}

/* Given an instruction corresponding to a call, validate that the generated
 * return address is safe.
 * Parameters:
 *   vstate - The state of the validator.
 */
static void NaClValidateCallAlignment(NaClValidatorState* vstate) {
  /* The return is safe only if it begins at an aligned address (since
   * return instructions are not explicit jumps).
   */
  NaClPcAddress next_pc =
      NaClInstStateVpc(vstate->cur_inst_state) +
      NaClInstStateLength(vstate->cur_inst_state);
  if (next_pc & vstate->alignment_mask) {
    DEBUG(NaClLog(LOG_INFO,
                  "Call alignment: pc = %"NACL_PRIxNaClPcAddress", "
                 "next_pc=%"NACL_PRIxNaClPcAddress", "
                 "mask = %"NACL_PRIxNaClPcAddress"\n",
                  NaClInstStateVpc(vstate->cur_inst_state),
                  next_pc, vstate->alignment_mask));
    NaClValidatorInstMessage(
        LOG_ERROR, vstate, vstate->cur_inst_state,
        "Bad call alignment, return pc = %"NACL_PRIxNaClPcAddress"\n", next_pc);
  }
}

/* Record that the given address of the given instruction is the beginning of
 * a disassembled instruction.
 * Parameters:
 *   vstate - The state of the validator.
 *   pc - The (virtual) address of a disassembled instruction.
 */
static void NaClRememberIp(NaClValidatorState* vstate,
                           NaClPcAddress pc) {
  if (pc < vstate->vbase || pc >= vstate->vbase + vstate->codesize) {
    NaClValidatorInstMessage(LOG_ERROR, vstate, vstate->cur_inst_state,
                             "Instruction pc out of range\n");
  } else {
    DEBUG(NaClLog(LOG_INFO,
                  "Add possible jump address: %"NACL_PRIxNaClPcAddress"\n",
                  pc));
    NaClAddressSetAddInline(vstate->jump_sets.possible_targets, pc, vstate);
  }
}

void NaClJumpValidatorRememberIpOnly(NaClValidatorState* vstate) {
  NaClPcAddress pc = NaClInstStateVpc(vstate->cur_inst_state);
  NaClRememberIp(vstate, pc);
}

void NaClJumpValidator(NaClValidatorState* vstate) {
  NaClRememberIp(vstate, NaClInstStateVpc(vstate->cur_inst_state));;
  if (vstate->cur_inst->flags &
      (NACL_IFLAG(JumpInstruction) | NACL_IFLAG(ConditionalJump))) {
    NaClAddExprJumpTarget(vstate);
    if (vstate->cur_inst->name == InstCall) {
      NaClValidateCallAlignment(vstate);
    }
  }
}

/* Returns true if the given address corresponds to the beginning
 * of an atomic sequence of instructions, and hence can be branched to.
 */
static Bool IsNaClReachableAddress(NaClValidatorState* vstate,
                                   NaClPcAddress addr) {
  DEBUG(NaClLog(LOG_INFO, "possible contains: %d\n",
                (int) NaClAddressSetContains(vstate->jump_sets.possible_targets,
                                             addr, vstate)));
  DEBUG(NaClLog(LOG_INFO, "removed contains: %d\n",
                (int) NaClAddressSetContains(vstate->jump_sets.removed_targets,
                                             addr, vstate)));
  return NaClAddressSetContains(vstate->jump_sets.possible_targets,
                                addr, vstate) &&
      !NaClAddressSetContains(vstate->jump_sets.removed_targets, addr, vstate);
}

void NaClJumpValidatorSummarize(NaClValidatorState* vstate) {
  /* Check that any explicit jump is to a possible (atomic) sequence
   * of disassembled instructions.
   */
  NaClJumpSets* jump_sets;
  NaClPcAddress addr;
  size_t i;
  if (vstate->quit) return;
  jump_sets = &vstate->jump_sets;
  NaClValidatorMessage(
      LOG_INFO, vstate,
      "Checking jump targets: %"NACL_PRIxNaClPcAddress
      " to %"NACL_PRIxNaClPcAddress"\n",
      vstate->vbase, vstate->vbase + vstate->codesize);

  /* Note: The following is the high-level code that has been commented
   * out, because it is too slow. It represents slightly more than
   * 15% of the overall validator time.
   *
  for (addr = vstate->vbase; addr < vstate->vbase + vstate->codesize; addr++) {
    if (NaClAddressSetContains(jump_sets->actual_targets, addr, vstate)) {
      DEBUG(NaClLog(LOG_INFO,
                    "Checking jump address: %"NACL_PRIxNaClPcAddress"\n",
                    addr));
      if (!IsNaClReachableAddress(vstate, addr, jump_sets)) {
        NaClValidatorPcAddressMessage(LOG_ERROR, vstate, addr,
                                      "Bad jump target\n");
      }
    }
  }
  */

  /* (Low level) Walk the collected sets to find address that correspond
   * to branches into an atomic sequence of instructions.
   */
  for (i = 0; i < jump_sets->set_array_size; ++i) {
    uint8_t problem = jump_sets->actual_targets[i] &
        (~jump_sets->possible_targets[i] |
         jump_sets->removed_targets[i]);
    if (problem) {
      /* Some bit in this range is a problem, so we will convert back
       * to code like the above and test each bit separately.
       */
      NaClPcAddress j;
      NaClPcAddress base = (i << 3) + vstate->vbase;
      for (j = 0; j < 8; ++j) {
        addr = base + j;
        if (addr < vstate->vbase + vstate->codesize) {
          if (NaClAddressSetContains(jump_sets->actual_targets, addr, vstate)) {
            DEBUG(NaClLog(LOG_INFO,
                          "Checking jump address: %"NACL_PRIxNaClPcAddress"\n",
                          addr));
            if (!IsNaClReachableAddress(vstate, addr)) {
              NaClValidatorPcAddressMessage(LOG_ERROR, vstate, addr,
                                            "Bad jump target\n");
            }
          }
        }
      }
    }
  }

  /* Check that all block boundaries are accessable at an aligned address. */
  NaClValidatorMessage(
      LOG_INFO, vstate, "Checking that basic blocks are aligned\n");
  if (vstate->vbase & vstate->alignment_mask) {
    NaClValidatorMessage(LOG_ERROR, vstate,
                         "Code segment starts at 0x%"NACL_PRIxNaClPcAddress", "
                         "which isn't aligned properly.\n",
                         vstate->vbase);
  } else {
    for (addr = vstate->vbase; addr < vstate->vbase + vstate->codesize;
         addr += vstate->alignment) {
      DEBUG(NaClLog(LOG_INFO,
                    "Checking block address: %"NACL_PRIxNaClPcAddress"\n",
                   addr));
      if (!IsNaClReachableAddress(vstate, addr)) {
        NaClValidatorPcAddressMessage(
            LOG_ERROR, vstate, addr, "Bad basic block alignment.\n");
      }
    }
  }
}

void NaClJumpValidatorCleanUp(NaClValidatorState* vstate) {
  if (NULL != vstate) {
    NaClJumpSets* jump_sets = &vstate->jump_sets;
    NaClAddressSetDestroy(jump_sets->actual_targets);
    NaClAddressSetDestroy(jump_sets->possible_targets);
    NaClAddressSetDestroy(jump_sets->removed_targets);
    jump_sets->actual_targets = NULL;
    jump_sets->possible_targets = NULL;
    jump_sets->removed_targets = NULL;
  }
}

static INLINE void NaClMarkInstructionJumpIllegalInline(
    struct NaClValidatorState* vstate,
    struct NaClInstState* inst) {
  NaClPcAddress pc = NaClInstStateVpc(inst);
  if (pc < vstate->vbase || pc >= vstate->vbase + vstate->codesize) {
    /* ERROR instruction out of range.
     * Note: Not reported here, because this will already be reported by
     * the call to NaClRememberIp in JumpValidator.
     */
  } else {
    DEBUG(NaClLog(LOG_INFO,
                  "Mark instruction as jump illegal: %"NACL_PRIxNaClPcAddress
                 "\n",
                 pc));
    NaClAddressSetAddInline(vstate->jump_sets.removed_targets, pc, vstate);
  }
}

void NaClMarkInstructionJumpIllegal(struct NaClValidatorState* vstate,
                                    struct NaClInstState* inst) {
  NaClMarkInstructionJumpIllegalInline(vstate, inst);
}

void NaClMarkInstructionsJumpRangeIllegal(struct NaClValidatorState* vstate,
                                          int distance) {
  int i;
  for (i = 0; i < distance; i++) {
    struct NaClInstState* inst =
        NaClInstIterGetLookbackStateInline(vstate->cur_iter, i);
    NaClMarkInstructionJumpIllegalInline(vstate, inst);
  }
}

void NaClMarkInstructionJumpIllegalLookback(
    struct NaClInstIter* iter,
    struct NaClValidatorState* state,
    size_t n) {
  NaClMarkInstructionJumpIllegal(
      state,
      (n == 0)
      ? state->cur_inst_state
      : NaClInstIterGetLookbackStateInline(iter, n));
}
