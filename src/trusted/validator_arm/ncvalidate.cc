/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/arm_validate.h"
#include "native_client/src/trusted/validator_arm/ncdecode.h"
#include "native_client/src/trusted/validator_arm/ncvalidate.h"
#include "native_client/src/trusted/validator_arm/branch_patterns.h"
#include "native_client/src/trusted/validator_arm/sequestered_reg_patterns.h"
#include "native_client/src/trusted/validator_arm/stack_adjust_patterns.h"
#include "native_client/src/trusted/validator_arm/store_patterns.h"

void NCValidateInit() {
  InstallBranchPatterns();
  InstallStackAdjustPatterns();
  InstallStorePatterns();
  InstallSequesteredRegisterPatterns();
}

void NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t size) {
  CodeSegment code_segment;
  CodeSegmentInitialize(&code_segment, mbase, vbase, size);
  ValidateCodeSegment(&code_segment);
}

int NCValidateFinish() {
  return  ValidateExitCode();
}

