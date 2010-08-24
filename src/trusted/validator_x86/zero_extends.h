/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_ZERO_EXTENDS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_ZERO_EXTENDS_H_

/* Looks the current instruction up in a table of instructions that
 * zero extends 32-bit registers to 64-bit registers, and adds corresponding
 * instruction flag to OpDest operands, if applicable.
 */
void NaClAddZeroExtend32FlagIfApplicable();

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_ZERO_EXTENDS_H_ */
