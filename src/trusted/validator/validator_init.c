/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"


/* The function is not static to avoid compiler error on platforms where it is
 * not used.
 */
void EmitExperimentalValidatorWarning() {
  NaClLog(LOG_WARNING, "DANGER! USING THE EXPERIMENTAL DFA VALIDATOR!\n");
}

const struct NaClValidatorInterface *NaClCreateValidator() {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  return NaClValidatorCreateArm();
#elif NACL_ARCH(NACL_TARGET_ARCH) == NACL_x86
# if NACL_TARGET_SUBARCH == 64
#  if defined(NACL_VALIDATOR_RAGEL)
  EmitExperimentalValidatorWarning();
  return NaClDfaValidatorCreate_x86_64();
#  else
  return NaClValidatorCreate_x86_64();
#  endif  /* defined(NACL_VALIDATOR_RAGEL) */
# elif NACL_TARGET_SUBARCH == 32
#  if defined(NACL_VALIDATOR_RAGEL)
  EmitExperimentalValidatorWarning();
  return NaClDfaValidatorCreate_x86_32();
#  else
  return NaClValidatorCreate_x86_32();
#  endif  /* defined(NACL_VALIDATOR_RAGEL) */
# else
#  error "Invalid sub-architecture!"
# endif
#else  /* NACL_x86 */
# error "There is no validator for this architecture!"
#endif
}
