/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_DEF_JUMPS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_DEF_JUMPS_H__

/*
 * Looks up the current instruction in a table of instructions that
 * define jumps, and adds corresponding instruction flags to the
 * instruction.
 */

void NaClAddJumpFlagsIfApplicable(void);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NC_DEF_JUMPS_H__ */
