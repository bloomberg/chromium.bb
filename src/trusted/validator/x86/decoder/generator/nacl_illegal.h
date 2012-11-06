/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NACL_ILLEGAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NACL_ILLEGAL_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

/* Looks at the current instruction up, in a table of intructions
 * that aren't legal in native client, and adds the corresponding
 * instruction flag if applicable.
 *
 * TODO(karl) This table is incomplete. As we fix instructions to use the new
 * generator model, this table will be extended.
 */
void NaClAddNaClIllegalIfApplicable(void);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NACL_ILLEGAL_H_ */
