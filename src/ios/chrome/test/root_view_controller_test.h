// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_ROOT_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_TEST_ROOT_VIEW_CONTROLLER_TEST_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "testing/platform_test.h"

// Base test fixture that restores the key window's original root view
// controller at tear down.
class RootViewControllerTest : public PlatformTest {
 public:
  RootViewControllerTest() = default;
  ~RootViewControllerTest() override = default;

 protected:
  // Sets the current key window's rootViewController and saves a pointer to
  // the original VC to allow restoring it at the end of the test.
  void SetRootViewController(UIViewController* new_root_view_controller);

  // PlatformTest implementation.
  void TearDown() override;

 private:
  // The key window's original root view controller, which must be restored at
  // the end of the test.
  UIViewController* original_root_view_controller_;

  DISALLOW_COPY_AND_ASSIGN(RootViewControllerTest);
};

#endif  // IOS_CHROME_TEST_ROOT_VIEW_CONTROLLER_TEST_H_
