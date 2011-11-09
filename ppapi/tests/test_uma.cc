// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_uma.h"

#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UMA);

bool TestUMA::Init() {
  uma_interface_ = reinterpret_cast<PPB_UMA_Private const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_UMA_PRIVATE_INTERFACE));
  return !!uma_interface_;
}

void TestUMA::RunTest() {
  instance_->LogTest("Count", TestCount());
  instance_->LogTest("Time", TestTime());
  instance_->LogTest("Enum", TestEnum());
}

std::string TestUMA::TestCount() {
  pp::Var name_var = pp::Var("Test.CountHistogram");
  PP_Var name = name_var.pp_var();
  uma_interface_->HistogramCustomCounts(name, 10, 1, 100, 50);
  uma_interface_->HistogramCustomCounts(name, 30, 1, 100, 50);
  uma_interface_->HistogramCustomCounts(name, 20, 1, 100, 50);
  uma_interface_->HistogramCustomCounts(name, 40, 1, 100, 50);
  PASS();
}

std::string TestUMA::TestTime() {
  pp::Var name_var = pp::Var("Test.TimeHistogram");
  PP_Var name = name_var.pp_var();
  uma_interface_->HistogramCustomTimes(name, 100, 1, 10000, 50);
  uma_interface_->HistogramCustomTimes(name, 1000, 1, 10000, 50);
  uma_interface_->HistogramCustomTimes(name, 5000, 1, 10000, 50);
  uma_interface_->HistogramCustomTimes(name, 10, 1, 10000, 50);
  PASS();
}

std::string TestUMA::TestEnum() {
  pp::Var name_var = pp::Var("Test.EnumHistogram");
  PP_Var name = name_var.pp_var();
  uma_interface_->HistogramEnumeration(name, 0, 5);
  uma_interface_->HistogramEnumeration(name, 3, 5);
  uma_interface_->HistogramEnumeration(name, 3, 5);
  uma_interface_->HistogramEnumeration(name, 1, 5);
  uma_interface_->HistogramEnumeration(name, 2, 5);
  PASS();
}

