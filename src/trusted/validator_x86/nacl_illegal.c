/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Captures instructions that are considered illegal in native client.
 *
 * Note: This is used by the x86-64 validator to decide what instructions
 * should be flagged as illegal.
 */

#include "native_client/src/trusted/validator_x86/nacl_illegal.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* List of instruction mnemonics that are illegal. */
static const NaClMnemonic kNaClIllegalOp[] = {
  /* TODO(karl) This list is incomplete. As we fix instructions to use the new
   * generator model, this list will be extended.
   */
  InstDas,
};

void NaClAddNaClIllegalIfApplicable() {
  Bool is_illegal = FALSE;  /* until proven otherwise. */
  NaClInst* inst = NaClGetDefInst();

  /* TODO(karl) Once all instructions have been modified to be explicitly
   * marked as illegal, remove the corresponding switch from nc_illegal.c,
   * since it will no longer be needed.
   * Note: As instructions are modified to use the new generator model,
   * The file testdata/64/modeled_insts.txt will reflect it by showing
   * the NaClIllegal flag.
   */
  /* Be sure to handle instruction groups we don't allow. */
  switch (inst->insttype) {
    case NACLi_RETURN:
    case NACLi_EMMX:
      /* EMMX needs to be supported someday but isn't ready yet. */
    case NACLi_ILLEGAL:
    case NACLi_SYSTEM:
    case NACLi_RDMSR:
    case NACLi_RDTSCP:
    case NACLi_LONGMODE:
    case NACLi_SVM:
    case NACLi_3BYTE:
    case NACLi_CMPXCHG16B:
    case NACLi_UNDEFINED:
    case NACLi_INVALID:
    case NACLi_SYSCALL:
    case NACLi_SYSENTER:
      is_illegal = TRUE;
      break;
    default:
      if (NaClInInstructionSet(kNaClIllegalOp, NACL_ARRAY_SIZE(kNaClIllegalOp),
                               NULL, 0)) {
        is_illegal = TRUE;
      }
      break;
  }
  if (is_illegal) {
    NaClAddIFlags(NACL_IFLAG(NaClIllegal));
  }
}
