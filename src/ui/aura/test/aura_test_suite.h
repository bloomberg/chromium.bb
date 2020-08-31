// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_SUITE_H_
#define UI_AURA_TEST_AURA_TEST_SUITE_H_

#include "base/macros.h"

namespace aura {
namespace test {

// The aura test suite is configured in such a way that Env is shared across
// all tests. If a test needs to install a fresh copy of env it can create an
// instance of this. The constructor destroys the global instance, and the
// destructor reinstates it.
// Typical usage is:
//    MyTest::SetUp() {
//      env_reinstaller_ = std::make_unique<EnvReinstaller>();
//      my_test_env_ = Env::CreateInstance()
//      AuraTestBase::SetUp();
//    }
//    MyTest::TearDown() {
//      AuraTestBase::TearDown();
//      my_test_env_.reset();
//      env_reinstaller_.reset();
//    }
// TODO(sky): this is ugly. Instead look into having each test install it's own
// Env. https://crbug.com/822968
class EnvReinstaller {
 public:
  EnvReinstaller();
  ~EnvReinstaller();

 private:
  DISALLOW_COPY_AND_ASSIGN(EnvReinstaller);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_SUITE_H_
