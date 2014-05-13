// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

#include "base/memory/scoped_ptr.h"
#include "base/test/test_suite.h"
#include "testing/gtest_mac.h"

#import "remoting/ios/Chromoting_unittests/main_no_arc.h"

int main(int argc, char* argv[]) {
  // Initialization that must occur with no Automatic Reference Counting (ARC)
  remoting::main_no_arc::init();

  testing::InitGoogleTest(&argc, argv);
  scoped_ptr<base::TestSuite> runner(new base::TestSuite(argc, argv));
  runner.get()->Run();
}
