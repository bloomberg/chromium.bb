/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_VS_BASELINE_h_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_VS_BASELINE_h_

#include "native_client/src/trusted/validator_arm/actual_vs_baseline.h"

namespace nacl_arm_test {

// This file defines a tester that compares the "hand-written" baseline
// instruction decoders to the "generated" baseline instruction decoders.
// It does this by testing for each decoded match, whether the generated
// baseline decoders behave the same. If so, there are interchangable.
class BaselineVsBaselineTester : public ActualVsBaselineTester {
 public:
  BaselineVsBaselineTester(const NamedClassDecoder& gen_baseline,
                           DecoderTester& hand_baseline_tester);

 protected:
  // We override sanity checks, assuming that they have already been
  // applied when testing the (hand-coded) baseline decoders against
  // the baseline decoder.
  virtual bool DoApplySanityChecks();

  // We override the CheckDefs method to allow us to test that
  // each baseline implies the other.
  virtual void CheckDefs();
};

}  // namespace nacl_arm_test

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_BASELINE_VS_BASELINE_h_
