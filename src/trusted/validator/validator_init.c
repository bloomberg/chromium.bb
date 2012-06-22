/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/ncvalidate.h"

int NaClUseDfaValidator() {
  if (getenv("NACL_DANGEROUS_USE_DFA_VALIDATOR") != NULL) {
    return 1;
  }
  return 0;
}

const struct NaClValidatorInterface *NaClCreateValidator() {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  return NaClValidatorCreateArm();
#elif NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
#error "No validator available for this architecture!"
#elif NACL_TARGET_SUBARCH == 32 && defined(NACL_STANDALONE)
  if (NaClUseDfaValidator()) {
    NaClLog(LOG_WARNING, "DANGER! USING THE EXPERIMENTAL DFA VALIDATOR!\n");
    return NaClDfaValidatorCreate_x86_32();
  } else {
    return NaClValidatorCreate_x86_32();
  }
#elif NACL_TARGET_SUBARCH == 32
  return NaClValidatorCreate_x86_32();
#elif NACL_TARGET_SUBARCH == 64 && defined(NACL_STANDALONE)
  if (NaClUseDfaValidator()) {
    NaClLog(LOG_WARNING, "DANGER! USING THE EXPERIMENTAL DFA VALIDATOR!\n");
    return NaClDfaValidatorCreate_x86_64();
  } else {
    return NaClValidatorCreate_x86_64();
  }
#elif NACL_TARGET_SUBARCH == 64
  return NaClValidatorCreate_x86_64();
#endif
}
