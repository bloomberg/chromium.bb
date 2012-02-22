/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Internal functions to be used to test single instruction parsing
 * in private_insts/enuminsts.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCDECODE_AUX_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCDECODE_AUX_H_

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"

void NCDecoderStateNewSegment(NCDecoderState* tthis);
void NCConsumeNextInstruction(struct NCDecoderInst* inst);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCDECODE_AUX_H_ */
