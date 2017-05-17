// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkPerfTestSuite_h
#define BlinkPerfTestSuite_h

#include "base/test/test_suite.h"

namespace blink {

class BlinkPerfTestSuite : public base::TestSuite {
 public:
  BlinkPerfTestSuite(int argc, char** argv);

  void Initialize() override;
  void Shutdown() override;
};

}  // namespace blink

#endif  // BlinkPerfTestSuite_h
