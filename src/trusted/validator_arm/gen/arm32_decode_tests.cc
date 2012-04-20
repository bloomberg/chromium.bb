

/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif



#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

namespace nacl_arm_test {


class Add_Rule_7_A1_P26Binary4RegisterShiftedOpTesterRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Add_Rule_7_A1_P26Binary4RegisterShiftedOpTesterRegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Add_Rule_7_A1_P26Binary4RegisterShiftedOp_instance_)
  {}
};

class Rsb_Rule_144_A1_P288Binary4RegisterShiftedOpTesterRegsNotPc
    : public Binary4RegisterShiftedOpTesterRegsNotPc {
 public:
  Rsb_Rule_144_A1_P288Binary4RegisterShiftedOpTesterRegsNotPc()
    : Binary4RegisterShiftedOpTesterRegsNotPc(
      state_.Rsb_Rule_144_A1_P288Binary4RegisterShiftedOp_instance_)
  {}
};

// Defines a gtest testing harness for tests.
class Arm32DecoderStateTests : public ::testing::Test {
 protected:
  Arm32DecoderStateTests() {}
};

TEST_F(Arm32DecoderStateTests,
       Add_Rule_7_A1_P26Binary4RegisterShiftedOpRegsNotPc_cccc0000100snnnnddddssss0tt1mmmm_Test) {
  Add_Rule_7_A1_P26Binary4RegisterShiftedOpTesterRegsNotPc tester;
  tester.Test("cccc0000100snnnnddddssss0tt1mmmm");
}

TEST_F(Arm32DecoderStateTests,
       Rsb_Rule_144_A1_P288Binary4RegisterShiftedOpRegsNotPc_cccc0000011snnnnddddssss0tt1mmmm_Test) {
  Rsb_Rule_144_A1_P288Binary4RegisterShiftedOpTesterRegsNotPc tester;
  tester.Test("cccc0000011snnnnddddssss0tt1mmmm");
}

}  // namespace nacl_arm_test

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
