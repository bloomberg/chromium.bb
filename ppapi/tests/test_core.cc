// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_core.h"

#if defined(_MSC_VER)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "ppapi/cpp/core.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

namespace {

void PlatformSleep(int duration_ms) {
#if defined(_MSC_VER)
  ::Sleep(duration_ms);
#else
  usleep(duration_ms * 1000);
#endif
}

}  // namespace

REGISTER_TEST_CASE(Core);

bool TestCore::Init() {
  return true;
}

void TestCore::RunTest() {
  RUN_TEST(Time);
  RUN_TEST(TimeTicks);
}

std::string TestCore::TestTime() {
  pp::Core* core = pp::Module::Get()->core();
  PP_Time time1 = core->GetTime();
  ASSERT_TRUE(time1 > 0);

  PlatformSleep(100);  // 0.1 second

  PP_Time time2 = core->GetTime();
  ASSERT_TRUE(time2 > time1);

  PASS();
}

std::string TestCore::TestTimeTicks() {
  pp::Core* core = pp::Module::Get()->core();
  PP_Time time1 = core->GetTimeTicks();
  ASSERT_TRUE(time1 > 0);

  PlatformSleep(100);  // 0.1 second

  PP_Time time2 = core->GetTimeTicks();
  ASSERT_TRUE(time2 > time1);

  PASS();
}

