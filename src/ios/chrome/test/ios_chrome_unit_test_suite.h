// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_
#define IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "ios/web/public/test/web_test_suite.h"

// Test suite for unit tests.
class IOSChromeUnitTestSuite : public web::WebTestSuite {
 public:
  IOSChromeUnitTestSuite(int argc, char** argv);
  ~IOSChromeUnitTestSuite() override;

  // web::WebTestSuite overrides:
  void Initialize() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> action_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeUnitTestSuite);
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_
