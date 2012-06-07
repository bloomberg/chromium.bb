// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_TEST_APP_LIST_TEST_SUITE_H_
#define UI_APP_LIST_TEST_APP_LIST_TEST_SUITE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/test/test_suite.h"

namespace app_list {
namespace test {

class AppListTestSuite : public base::TestSuite {
 public:
  AppListTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite overrides:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListTestSuite);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_TEST_APP_LIST_TEST_SUITE_H_
