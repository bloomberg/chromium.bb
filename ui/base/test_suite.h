// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SUITE_H_
#define UI_BASE_TEST_SUITE_H_
#pragma once

#include "build/build_config.h"

#include "base/path_service.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

class UiBaseTestSuite : public base::TestSuite {
 public:
  UiBaseTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

 protected:

  virtual void Initialize() {
    base::mac::ScopedNSAutoreleasePool autorelease_pool;

    TestSuite::Initialize();
  }

  virtual void Shutdown() {
    TestSuite::Shutdown();
  }
};

#endif  // UI_BASE_TEST_SUITE_H_
