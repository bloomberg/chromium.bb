/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_LONG_MODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_LONG_MODE_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

/* Add LongMode instruction flag to the current instruction, if applicable. */
void NaClAddLongModeIfApplicable(void);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_LONG_MODE_H_ */
