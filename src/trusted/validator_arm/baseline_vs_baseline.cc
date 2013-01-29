/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/baseline_vs_baseline.h"

#include "gtest/gtest.h"

namespace nacl_arm_test {

BaselineVsBaselineTester:: BaselineVsBaselineTester(
    const NamedClassDecoder& gen_baseline,
    DecoderTester& hand_baseline_tester)
    : ActualVsBaselineTester(gen_baseline, hand_baseline_tester) {}

bool BaselineVsBaselineTester::DoApplySanityChecks() {
  return true;
}

void BaselineVsBaselineTester::CheckDefs() {
  EXPECT_TRUE(baseline_decoder_.defs(inst_).
              Equals(actual_decoder_.defs(inst_)));
}

}  // namespace nacl_arm_test
