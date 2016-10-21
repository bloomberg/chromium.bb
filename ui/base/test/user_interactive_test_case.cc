// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/user_interactive_test_case.h"

#include "base/run_loop.h"

namespace test {

void RunTestInteractively() {
  base::RunLoop().Run();
}

}  // namespace test
