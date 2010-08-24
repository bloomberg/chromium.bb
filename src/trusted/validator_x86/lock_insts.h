/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_LOCK_INSTS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_LOCK_INSTS_H_

/* Looks at the current instruction up in a table of instructions
 * that can have a lock prefix, and adds corresponding
 * instruciton flag if applicable.
 */
void NaClLockableFlagIfApplicable();

#endif  /*  NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_LOCK_INSTS_H_ */
