/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DEFSIZE64_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DEFSIZE64_H_

/* Looks the current instruction up in a table of instructions that
 * assume the default operand size is 64 bits instead of 32 bits.
 */
void NaClAddSizeDefaultIs64();

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DEFSIZE64_H_ */
