/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_REP_PREFIX_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_REP_PREFIX_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

/* Looks the current instruction up in a table of instructions that
 * can have REP/REPE/REPZ (F3) and REPNE/REPNZ (F2) prefix bytes, and
 * adds corresponding instruction flags if applicable.
 */
void NaClAddRepPrefixFlagsIfApplicable(void);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_REP_PREFIX_H_ */
