// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "ui/compositor/test/test_suite.h"

int main(int argc, char** argv) {
#if defined(USE_AURA)
  return ui::test::CompositorTestSuite(argc, argv).Run();
#else
  return base::TestSuite(argc, argv).Run();
#endif
}
