/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCCOPYCODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCCOPYCODE_H_

/* Copies a single instruction, avoiding the possibility of other threads
 * executing a partially changed instruction.
 */
int NaClCopyInstructionX86(uint8_t *dst, uint8_t *src, uint8_t sz);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_COPYCODE_H_ */
