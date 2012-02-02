/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_postconds.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/ncop_exps.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_trans.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_utils.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/nc_memory_protect.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

#include "native_client/src/trusted/validator/x86/decoder/ncop_exps_inl.c"

/* Maximum character buffer size to use for generating messages. */
static const size_t kMaxBufferSize = 1024;

#ifdef NCVAL_TESTING
void NaClAddAssignsRegisterWithZeroExtendsPostconds(
    struct NaClValidatorState* state) {
  uint32_t i;
  NaClExpVector* vector = state->cur_inst_vector;

  DEBUG(NaClValidatorInstMessage(
      LOG_INFO, state, state->cur_inst_state,
      "-> Checking ZeroExtends postconditions...\n"));

  /* Look for assignments to 32-bit registers for instructions that
   * zero extend.
   */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    NaClExp* node = &vector->node[i];
    NaClOpKind node_reg;
    if (ExprRegister != node->kind) continue;
    if (!NaClHasBit(node->flags, NACL_EFLAG(ExprSet))) continue;
    if (!NaClHasBit(node->flags, NACL_EFLAG(ExprSize32))) continue;
    node_reg = NaClGetExpRegisterInline(node);
    if (node_reg == RegUnknown) continue;
    if (NaClAssignsRegisterWithZeroExtends32(state, 0, node_reg)) {
      char* buffer;
      size_t buffer_size;
      char reg_name[kMaxBufferSize];
      NaClOpRegName(node_reg, reg_name, kMaxBufferSize);
      NaClConditionAppend(state->postcond, &buffer, &buffer_size);
      SNPRINTF(buffer, buffer_size, "ZeroExtends(%s)", reg_name);
    }
  }
  DEBUG(NaClValidatorMessage(
      LOG_INFO, state, "<- Finished ZeroExtends postconditions...\n"));
}

void NaClAddLeaSafeAddressPostconds(
    struct NaClValidatorState* state) {
  uint32_t i;
  NaClExpVector* vector = state->cur_inst_vector;
  DEBUG(NaClValidatorInstMessage(
      LOG_INFO, state, state->cur_inst_state,
      "Checking SafeAddress postconditions...\n"));

  /* Look for assignments to registers. */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    NaClOpKind reg;
    NaClExp* node = &vector->node[i];
    if (ExprRegister != node->kind) continue;
    if (!NaClHasBit(node->flags, NACL_EFLAG(ExprSet))) continue;
    if (!NaClHasBit(node->flags, NACL_EFLAG(ExprSize64))) continue;
    reg = NaClGetExpRegisterInline(node);
    if (RegUnknown == reg) continue;
    if ((reg == RegRSP) || (reg == RegRBP)) {
      /* Note: Do not need to check safe addresses computed
       * by "LEA Rsp/Rbp ...". Currently, these two registers
       * are already checked.. See NaClCheckRspAssignments and
       * NaClCheckRbpAssignments in nc_protect_base.c for more
       * information on how such LEA instructions are checked.
       */
      if (InstLea != NaClInstStateInst(state->cur_inst_state)->name) {
        NaClAcceptLeaWithMoveLea32To64(state, reg);
      }
    } else if (NaClAcceptLeaSafeAddress(state)) {
      char* buffer;
      size_t buffer_size;
      char reg_name[kMaxBufferSize];
      NaClOpRegName(reg, reg_name, kMaxBufferSize);
      NaClConditionAppend(state->postcond, &buffer, &buffer_size);
      SNPRINTF(buffer, buffer_size, "SafeAddress(%s)", reg_name);
    }
  }
  DEBUG(NaClValidatorMessage(
      LOG_INFO, state, "Finished SafeAddress postconditions...\n"));
}

#endif
